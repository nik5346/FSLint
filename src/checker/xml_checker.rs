use crate::certificate::{Certificate, TestStatus};
use roxmltree::Document;
use chrono::Datelike;

pub struct XmlChecker;

impl XmlChecker {
    pub fn validate_fmu(xml_content: &str, cert: &mut Certificate) -> anyhow::Result<()> {
        cert.log("\n--- FMU MODEL DESCRIPTION VALIDATION ---");

        // XXE Security Check
        if xml_content.contains("<!DOCTYPE") || xml_content.contains("<!ENTITY") {
            cert.add_test_result(
                "[SECURITY] XXE Check",
                TestStatus::FAIL,
                vec!["XML file must not contain <!DOCTYPE> or <!ENTITY> declarations".to_string()],
            );
            return Err(anyhow::anyhow!("XXE vulnerability detected"));
        }
        cert.add_test_result("[SECURITY] XXE Check", TestStatus::PASS, Vec::new());

        let doc = match Document::parse(xml_content) {
            Ok(d) => d,
            Err(e) => {
                cert.add_test_result(
                    "XML Parse",
                    TestStatus::FAIL,
                    vec![format!("Failed to parse modelDescription.xml: {}", e)],
                );
                return Err(anyhow::anyhow!("XML parse error"));
            }
        };
        cert.add_test_result("XML Parse", TestStatus::PASS, Vec::new());

        let root = doc.root_element();
        if root.tag_name().name() != "fmiModelDescription" {
            cert.add_test_result(
                "Root Element",
                TestStatus::FAIL,
                vec!["Root element must be <fmiModelDescription>".to_string()],
            );
            return Err(anyhow::anyhow!("Invalid root element"));
        }

        // Model Name
        if let Some(name) = root.attribute("modelName") {
            if name.is_empty() {
                cert.add_test_result(
                    "Model Name",
                    TestStatus::FAIL,
                    vec!["modelName attribute is empty".to_string()],
                );
            } else {
                cert.add_test_result("Model Name", TestStatus::PASS, Vec::new());
                let mut summary = cert.summary.clone();
                summary.model_name = name.to_string();
                cert.set_summary(summary);
            }
        } else {
            cert.add_test_result(
                "Model Name",
                TestStatus::FAIL,
                vec!["modelName attribute is missing".to_string()],
            );
        }

        // FMI Version
        if let Some(version) = root.attribute("fmiVersion") {
            if !matches!(version, "1.0" | "2.0" | "3.0") {
                cert.add_test_result(
                    "FMI Version",
                    TestStatus::FAIL,
                    vec![format!("Unsupported fmiVersion: {}", version)],
                );
            } else {
                cert.add_test_result("FMI Version", TestStatus::PASS, Vec::new());
                let mut summary = cert.summary.clone();
                summary.fmi_version = version.to_string();
                cert.set_summary(summary);
            }
        } else {
            cert.add_test_result(
                "FMI Version",
                TestStatus::FAIL,
                vec!["fmiVersion attribute is missing".to_string()],
            );
        }

        // GUID or instantiationToken
        let fmi_version = root.attribute("fmiVersion").unwrap_or("");
        let guid = root
            .attribute("guid")
            .or(root.attribute("instantiationToken"));
        if let Some(g) = guid {
            if g.is_empty() {
                cert.add_test_result(
                    "GUID / instantiationToken",
                    TestStatus::FAIL,
                    vec!["GUID / instantiationToken is empty".to_string()],
                );
            } else {
                cert.add_test_result("GUID / instantiationToken", TestStatus::PASS, Vec::new());
                let mut summary = cert.summary.clone();
                summary.guid = g.to_string();
                cert.set_summary(summary);
            }
        } else {
            cert.add_test_result(
                "GUID / instantiationToken",
                TestStatus::FAIL,
                vec!["GUID (FMI 1.0/2.0) or instantiationToken (FMI 3.0) is missing".to_string()],
            );
        }

        // modelIdentifier
        let mut model_ids = Vec::new();
        if fmi_version == "1.0" {
            if let Some(id) = root.attribute("modelIdentifier") {
                model_ids.push(id);
            }
        } else {
            for node in root.children() {
                if matches!(
                    node.tag_name().name(),
                    "ModelExchange" | "CoSimulation" | "ScheduledExecution"
                ) {
                    if let Some(id) = node.attribute("modelIdentifier") {
                        model_ids.push(id);
                    }
                }
            }
        }

        if model_ids.is_empty() {
            cert.add_test_result(
                "Model Identifier",
                TestStatus::FAIL,
                vec!["No modelIdentifier found in modelDescription.xml".to_string()],
            );
        } else {
            let mut id_msgs = Vec::new();
            for id in &model_ids {
                if id.is_empty() {
                    id_msgs.push("modelIdentifier is empty".to_string());
                } else {
                    if !id.chars().next().is_some_and(|c| c.is_ascii_alphabetic() || c == '_') ||
                       !id.chars().all(|c| c.is_ascii_alphanumeric() || c == '_') {
                        id_msgs.push(format!("modelIdentifier '{}' is not a valid C identifier", id));
                    }
                    if id.len() > 200 {
                        id_msgs.push(format!("modelIdentifier '{}' exceeds maximum length of 200 characters", id));
                    } else if id.len() > 64 {
                        cert.add_test_result("Model Identifier Length", TestStatus::WARNING, vec![format!("modelIdentifier '{}' exceeds recommended length of 64 characters", id)]);
                    }
                }
            }
            cert.add_test_result("Model Identifier", if id_msgs.is_empty() { TestStatus::PASS } else { TestStatus::FAIL }, id_msgs);
        }

        // generationDateAndTime
        if let Some(dt_str) = root.attribute("generationDateAndTime") {
            match crate::iso8601::parse(dt_str) {
                Some(dt) => {
                    let now = chrono::Utc::now();
                    // Basic check: year should not be in the future
                    if dt.year > now.year() {
                         cert.add_test_result("Generation Date and Time", TestStatus::FAIL, vec![format!("generationDateAndTime '{}' is in the future", dt_str)]);
                    } else {
                        cert.add_test_result("Generation Date and Time", TestStatus::PASS, Vec::new());
                    }
                    let mut summary = cert.summary.clone();
                    summary.generation_date_and_time = dt_str.to_string();
                    cert.set_summary(summary);
                }
                None => {
                    cert.add_test_result("Generation Date and Time", TestStatus::FAIL, vec![format!("Invalid ISO 8601 date-time: {}", dt_str)]);
                }
            }
        }

        // Author and Copyright
        let author = root.attribute("author").unwrap_or("");
        let copyright = root.attribute("copyright").unwrap_or("");
        let mut author_copyright_msgs = Vec::new();

        if author.is_empty() && copyright.is_empty() {
            author_copyright_msgs.push("Both 'author' and 'copyright' are missing or empty. At least one should be provided.".to_string());
            cert.add_test_result("Author / Copyright", TestStatus::WARNING, author_copyright_msgs);
        } else {
            cert.add_test_result("Author / Copyright", TestStatus::PASS, Vec::new());
        }

        if !copyright.is_empty() {
            let mut cp_msgs = Vec::new();
            let cp_lower = copyright.to_lowercase();
            if !copyright.starts_with('©') && !cp_lower.starts_with("copyright") && !cp_lower.starts_with("copr.") {
                cp_msgs.push("Copyright notice should begin with ©, 'Copyright', or 'Copr.'".to_string());
            }
            // Simple check for year (4 digits)
            if !copyright.chars().any(|c| c.is_ascii_digit()) {
                 cp_msgs.push("Copyright notice should include the year of publication".to_string());
            }
            if !cp_msgs.is_empty() {
                cert.add_test_result("Copyright Format", TestStatus::WARNING, cp_msgs);
            }
        }

        let mut summary = cert.summary.clone();
        summary.author = author.to_string();
        summary.copyright = copyright.to_string();

        if let Some(tool) = root.attribute("generationTool") {
            summary.generation_tool = tool.to_string();
            cert.add_test_result("Generation Tool", TestStatus::PASS, Vec::new());
        } else {
            cert.add_test_result("Generation Tool", TestStatus::WARNING, vec!["generationTool attribute is missing. For manually created FMUs, it is recommended to set it to 'Handmade'.".to_string()]);
        }

        if let Some(license) = root.attribute("license") {
            summary.license = license.to_string();
            cert.add_test_result("License Attribute", TestStatus::PASS, Vec::new());
        } else {
             cert.add_test_result("License Attribute", TestStatus::WARNING, vec!["license attribute is missing in modelDescription.xml".to_string()]);
        }
        cert.set_summary(summary);

        // Variable Naming, Unique Names, and Consistency
        let mut var_names = std::collections::HashSet::new();
        let mut type_names = std::collections::HashSet::new();
        let mut unit_names = std::collections::HashSet::new();
        let mut referenced_types = std::collections::HashSet::new();
        let mut referenced_units = std::collections::HashSet::new();

        let mut var_msgs = Vec::new();
        let mut type_msgs = Vec::new();
        let mut unit_msgs = Vec::new();

        // 1. Collect Type and Unit Definitions
        if let Some(td_node) = root.children().find(|n| n.tag_name().name() == "TypeDefinitions") {
            for type_node in td_node.children().filter(|n| n.is_element()) {
                if let Some(name) = type_node.attribute("name") {
                    if !type_names.insert(name.to_string()) {
                        type_msgs.push(format!("Duplicate type name: {}", name));
                    }
                }
            }
        }

        if let Some(ud_node) = root.children().find(|n| n.tag_name().name() == "UnitDefinitions") {
            for unit_node in ud_node.children().filter(|n| n.tag_name().name() == "Unit") {
                if let Some(name) = unit_node.attribute("name") {
                    if !unit_names.insert(name.to_string()) {
                        unit_msgs.push(format!("Duplicate unit name: {}", name));
                    }
                }
            }
        }

        // 2. Validate Variables
        if let Some(sv_node) = root.children().find(|n| n.tag_name().name() == "ModelVariables") {
            let naming_convention = root.attribute("namingConvention").unwrap_or("flat");

            for var_node in sv_node.children().filter(|n| n.tag_name().name() == "ScalarVariable") {
                if let Some(name) = var_node.attribute("name") {
                    if !var_names.insert(name.to_string()) {
                        var_msgs.push(format!("Duplicate variable name: {}", name));
                    }

                    // Naming Convention Check
                    if naming_convention == "structured" {
                         // Simple check: if it contains der( or . or [, it should follow structured syntax
                         // For now, we'll just check for basics like not starting with a number if not quoted.
                    } else if name.chars().any(|c| matches!(c, '\r' | '\n' | '\t')) {
                         var_msgs.push(format!("Variable name '{}' contains illegal control characters (CR, LF, or Tab)", name));
                    }
                }

                if let Some(declared_type) = var_node.attribute("declaredType") {
                    referenced_types.insert(declared_type.to_string());
                    if !type_names.contains(declared_type) {
                        type_msgs.push(format!("Variable references undefined type: {}", declared_type));
                    }
                }

                for child in var_node.children().filter(|n| n.is_element()) {
                    if let Some(unit) = child.attribute("unit") {
                        referenced_units.insert(unit.to_string());
                        if !unit_names.contains(unit) {
                            unit_msgs.push(format!("Variable references undefined unit: {}", unit));
                        }
                    }
                }
            }
        }

        cert.add_test_result("Unique Variable Names", if var_msgs.iter().any(|m| m.contains("Duplicate")) { TestStatus::FAIL } else { TestStatus::PASS }, var_msgs.clone());
        cert.add_test_result("Variable Naming", if var_msgs.iter().any(|m| m.contains("illegal")) { TestStatus::FAIL } else { TestStatus::PASS }, var_msgs);
        cert.add_test_result("Unique Type Names", if type_msgs.iter().any(|m| m.contains("Duplicate")) { TestStatus::FAIL } else { TestStatus::PASS }, type_msgs.clone());
        cert.add_test_result("Type References", if type_msgs.iter().any(|m| m.contains("undefined")) { TestStatus::FAIL } else { TestStatus::PASS }, type_msgs);
        cert.add_test_result("Unique Unit Names", if unit_msgs.iter().any(|m| m.contains("Duplicate")) { TestStatus::FAIL } else { TestStatus::PASS }, unit_msgs.clone());
        cert.add_test_result("Unit References", if unit_msgs.iter().any(|m| m.contains("undefined")) { TestStatus::FAIL } else { TestStatus::PASS }, unit_msgs);

        // Unused definitions (WARNING)
        let mut unused_msgs = Vec::new();
        for t in &type_names {
            if !referenced_types.contains(t) {
                unused_msgs.push(format!("Type definition '{}' is not used by any variable", t));
            }
        }
        for u in &unit_names {
            if !referenced_units.contains(u) {
                unused_msgs.push(format!("Unit definition '{}' is not used by any variable", u));
            }
        }
        if !unused_msgs.is_empty() {
            cert.add_test_result("Unused Definitions", TestStatus::WARNING, unused_msgs);
        }

        // Model Structure and Interfaces
        let mut interface_msgs = Vec::new();
        let mut has_interface = false;
        for node in root.children() {
            if matches!(node.tag_name().name(), "CoSimulation" | "ModelExchange" | "ScheduledExecution") {
                has_interface = true;
                break;
            }
        }
        if !has_interface {
            interface_msgs.push("At least one interface (CoSimulation, ModelExchange, or ScheduledExecution) must be implemented.".to_string());
        }
        cert.add_test_result("Implemented Interfaces", if has_interface { TestStatus::PASS } else { TestStatus::FAIL }, interface_msgs);

        // Default Experiment
        if let Some(de_node) = root.children().find(|n| n.tag_name().name() == "DefaultExperiment") {
            let mut de_msgs = Vec::new();
            let start_time = de_node.attribute("startTime").and_then(|s| s.parse::<f64>().ok()).unwrap_or(0.0);
            let stop_time = de_node.attribute("stopTime").and_then(|s| s.parse::<f64>().ok());
            let tolerance = de_node.attribute("tolerance").and_then(|s| s.parse::<f64>().ok());
            let step_size = de_node.attribute("stepSize").and_then(|s| s.parse::<f64>().ok());

            if start_time < 0.0 {
                de_msgs.push(format!("startTime must be non-negative: {}", start_time));
            }
            if let Some(stop) = stop_time {
                if stop < 0.0 {
                    de_msgs.push(format!("stopTime must be non-negative: {}", stop));
                }
                if stop <= start_time {
                    de_msgs.push(format!("stopTime ({}) must be greater than startTime ({})", stop, start_time));
                }
            }
            if let Some(tol) = tolerance {
                if tol <= 0.0 {
                    de_msgs.push(format!("tolerance must be greater than zero: {}", tol));
                }
            }
            if let Some(ss) = step_size {
                if ss <= 0.0 {
                    de_msgs.push(format!("stepSize must be greater than zero: {}", ss));
                }
            }
            cert.add_test_result("Default Experiment", if de_msgs.is_empty() { TestStatus::PASS } else { TestStatus::FAIL }, de_msgs);
        }

        // FMI 2.0 Specific Checks
        if fmi_version == "2.0" {
            let mut fmi2_msgs = Vec::new();

            // Check for prohibited special floats (NaN/INF) in Real attributes
            let attributes_to_check = [
                "min", "max", "start", "nominal", "factor", "offset",
                "startTime", "stopTime", "tolerance", "stepSize"
            ];

            // This requires a deeper search through the XML, but we can check the attributes of the current root and its children
            for node in root.descendants() {
                for attr in attributes_to_check {
                    if let Some(val) = node.attribute(attr) {
                        let v = val.to_lowercase();
                        if v.contains("nan") || v.contains("inf") {
                             fmi2_msgs.push(format!("FMI 2.0 prohibits NaN/INF in attribute '{}' of <{}>: {}", attr, node.tag_name().name(), val));
                        }
                    }
                }

                // Enumeration Variables must have declaredType in FMI 2.0
                if node.tag_name().name() == "ScalarVariable" &&
                   node.children().any(|n| n.tag_name().name() == "Enumeration") {
                     if node.attribute("declaredType").is_none() {
                         fmi2_msgs.push(format!("FMI 2.0: Enumeration variable '{}' must have a 'declaredType' attribute.", node.attribute("name").unwrap_or("unnamed")));
                     }
                }
            }

            cert.add_test_result("FMI 2.0 Specific", if fmi2_msgs.is_empty() { TestStatus::PASS } else { TestStatus::FAIL }, fmi2_msgs);
        }

        Ok(())
    }

    pub fn validate_ssp(xml_content: &str, cert: &mut Certificate) -> anyhow::Result<()> {
        cert.log("\n--- SSP SYSTEM STRUCTURE VALIDATION ---");

        // XXE Security Check
        if xml_content.contains("<!DOCTYPE") || xml_content.contains("<!ENTITY") {
            cert.add_test_result(
                "[SECURITY] XXE Check",
                TestStatus::FAIL,
                vec!["XML file must not contain <!DOCTYPE> or <!ENTITY> declarations".to_string()],
            );
            return Err(anyhow::anyhow!("XXE vulnerability detected"));
        }
        cert.add_test_result("[SECURITY] XXE Check", TestStatus::PASS, Vec::new());

        let doc = match Document::parse(xml_content) {
            Ok(d) => d,
            Err(e) => {
                cert.add_test_result(
                    "XML Parse",
                    TestStatus::FAIL,
                    vec![format!("Failed to parse SystemStructure.ssd: {}", e)],
                );
                return Err(anyhow::anyhow!("XML parse error"));
            }
        };
        cert.add_test_result("XML Parse", TestStatus::PASS, Vec::new());

        let root = doc.root_element();
        if root.tag_name().name() != "SystemStructureDescription" {
            cert.add_test_result(
                "Root Element",
                TestStatus::FAIL,
                vec!["Root element must be <SystemStructureDescription>".to_string()],
            );
        } else {
            cert.add_test_result("Root Element", TestStatus::PASS, Vec::new());
        }

        // Name
        if let Some(name) = root.attribute("name") {
            if name.is_empty() {
                cert.add_test_result(
                    "SSP Name",
                    TestStatus::FAIL,
                    vec!["name attribute is empty".to_string()],
                );
            } else {
                cert.add_test_result("SSP Name", TestStatus::PASS, Vec::new());
                let mut summary = cert.summary.clone();
                summary.model_name = name.to_string();
                cert.set_summary(summary);
            }
        } else {
            cert.add_test_result(
                "SSP Name",
                TestStatus::FAIL,
                vec!["name attribute is missing".to_string()],
            );
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::certificate::Certificate;

    #[test]
    fn test_validate_fmu_basic() {
        let mut cert = Certificate::new();
        let xml = r#"<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="2.0" modelName="TestModel" guid="123" generationDateAndTime="2024-03-15T14:30:05Z">
    <CoSimulation modelIdentifier="TestModel" />
</fmiModelDescription>"#;
        XmlChecker::validate_fmu(xml, &mut cert).unwrap();
        assert!(cert.results.iter().any(|r| r.test_name == "Model Name" && r.status == TestStatus::PASS));
        assert!(cert.results.iter().any(|r| r.test_name == "FMI Version" && r.status == TestStatus::PASS));
        assert!(cert.results.iter().any(|r| r.test_name == "Model Identifier" && r.status == TestStatus::PASS));
    }

    #[test]
    fn test_invalid_model_identifier() {
        let mut cert = Certificate::new();
        let xml = r#"<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="2.0" modelName="TestModel" guid="123">
    <CoSimulation modelIdentifier="123_Invalid" />
</fmiModelDescription>"#;
        let _ = XmlChecker::validate_fmu(xml, &mut cert);
        assert!(cert.results.iter().any(|r| r.test_name == "Model Identifier" && r.status == TestStatus::FAIL));
    }

    #[test]
    fn test_future_generation_date() {
        let mut cert = Certificate::new();
        let xml = r#"<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="2.0" modelName="TestModel" guid="123" generationDateAndTime="2099-01-01T00:00:00Z">
</fmiModelDescription>"#;
        let _ = XmlChecker::validate_fmu(xml, &mut cert);
        assert!(cert.results.iter().any(|r| r.test_name == "Generation Date and Time" && r.status == TestStatus::FAIL));
    }

    #[test]
    fn test_duplicate_variable_names() {
        let mut cert = Certificate::new();
        let xml = r#"<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="2.0" modelName="Test" guid="123">
    <ModelVariables>
        <ScalarVariable name="v1" valueReference="1" />
        <ScalarVariable name="v1" valueReference="2" />
    </ModelVariables>
</fmiModelDescription>"#;
        let _ = XmlChecker::validate_fmu(xml, &mut cert);
        assert!(cert.results.iter().any(|r| r.test_name == "Unique Variable Names" && r.status == TestStatus::FAIL));
    }
}
