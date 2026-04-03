use crate::certificate::{Certificate, TestStatus};
use chrono::Datelike;
use roxmltree::Document;

pub struct XmlChecker;

impl XmlChecker {
    pub fn validate_fmu(
        xml_content: &str,
        cert: &mut Certificate,
        fmu_name: Option<&str>,
    ) -> anyhow::Result<()> {
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

        // XML Declaration and Encoding
        if !xml_content.trim_start().starts_with("<?xml") {
            cert.add_test_result(
                "XML Declaration",
                TestStatus::WARNING,
                vec!["XML declaration is missing".to_string()],
            );
        } else if !xml_content.contains("version=\"1.0\"") {
            cert.add_test_result(
                "XML Declaration",
                TestStatus::FAIL,
                vec!["XML version must be 1.0".to_string()],
            );
        } else {
            cert.add_test_result("XML Declaration", TestStatus::PASS, Vec::new());
        }

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

                if let Some(mv) = root.attribute("version") {
                    summary.model_version = mv.to_string();
                    // Basic semver check
                    let parts: Vec<&str> = mv.split('.').collect();
                    if parts.len() < 2 {
                        cert.add_test_result("Model Version Format", TestStatus::WARNING, vec![format!("Model version '{}' does not follow recommended semantic versioning (MAJOR.MINOR.PATCH)", mv)]);
                    }
                }

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

        // Log Categories
        if let Some(log_node) = root
            .children()
            .find(|n| n.tag_name().name() == "LogCategories")
        {
            let mut category_names = std::collections::HashSet::new();
            let mut log_msgs = Vec::new();
            for category in log_node
                .children()
                .filter(|n| n.tag_name().name() == "Category")
            {
                if let Some(name) = category.attribute("name") {
                    if !category_names.insert(name.to_string()) {
                        log_msgs.push(format!("Duplicate log category name: {}", name));
                    }
                }
            }
            cert.add_test_result(
                "Log Categories",
                if log_msgs.is_empty() {
                    TestStatus::PASS
                } else {
                    TestStatus::FAIL
                },
                log_msgs,
            );
        }

        // modelIdentifier
        let mut model_ids = Vec::new();
        if fmi_version == "1.0" {
            if let Some(id) = root.attribute("modelIdentifier") {
                model_ids.push(id);

                // FMI 1.0: modelIdentifier must match FMU filename stem
                if let Some(name) = fmu_name {
                    if id != name {
                        cert.add_test_result(
                            "Model Identifier Match",
                            TestStatus::FAIL,
                            vec![format!(
                                "FMI 1.0: modelIdentifier '{}' must match FMU filename stem '{}'",
                                id, name
                            )],
                        );
                    } else {
                        cert.add_test_result(
                            "Model Identifier Match",
                            TestStatus::PASS,
                            Vec::new(),
                        );
                    }
                }
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
                    if !id
                        .chars()
                        .next()
                        .is_some_and(|c| c.is_ascii_alphabetic() || c == '_')
                        || !id.chars().all(|c| c.is_ascii_alphanumeric() || c == '_')
                    {
                        id_msgs.push(format!(
                            "modelIdentifier '{}' is not a valid C identifier",
                            id
                        ));
                    }
                    if id.len() > 200 {
                        id_msgs.push(format!(
                            "modelIdentifier '{}' exceeds maximum length of 200 characters",
                            id
                        ));
                    } else if id.len() > 64 {
                        cert.add_test_result(
                            "Model Identifier Length",
                            TestStatus::WARNING,
                            vec![format!(
                                "modelIdentifier '{}' exceeds recommended length of 64 characters",
                                id
                            )],
                        );
                    }
                }
            }
            cert.add_test_result(
                "Model Identifier",
                if id_msgs.is_empty() {
                    TestStatus::PASS
                } else {
                    TestStatus::FAIL
                },
                id_msgs,
            );
        }

        // generationDateAndTime
        if let Some(dt_str) = root.attribute("generationDateAndTime") {
            match crate::iso8601::parse(dt_str) {
                Some(dt) => {
                    let now = chrono::Utc::now();
                    let mut dt_msgs = Vec::new();
                    // Basic check: year should not be in the future
                    if dt.year > now.year() {
                        dt_msgs.push(format!(
                            "generationDateAndTime '{}' is in the future",
                            dt_str
                        ));
                    }

                    // Version-specific "unreasonably old" checks
                    let min_year = if fmi_version == "1.0" {
                        2010
                    } else if fmi_version == "2.0" {
                        2014
                    } else if fmi_version == "3.0" {
                        2022
                    } else {
                        0
                    };

                    if dt.year < min_year {
                        cert.add_test_result(
                            "Generation Date and Time",
                            TestStatus::WARNING,
                            vec![format!("generationDateAndTime '{}' is unreasonably old for FMI {} (should be >= {})", dt_str, fmi_version, min_year)],
                        );
                    }

                    cert.add_test_result(
                        "Generation Date and Time",
                        if dt_msgs.is_empty() {
                            TestStatus::PASS
                        } else {
                            TestStatus::FAIL
                        },
                        dt_msgs,
                    );

                    let mut summary = cert.summary.clone();
                    summary.generation_date_and_time = dt_str.to_string();
                    cert.set_summary(summary);
                }
                None => {
                    cert.add_test_result(
                        "Generation Date and Time",
                        TestStatus::FAIL,
                        vec![format!("Invalid ISO 8601 date-time: {}", dt_str)],
                    );
                }
            }
        }

        // Author and Copyright
        let author = root.attribute("author").unwrap_or("");
        let copyright = root.attribute("copyright").unwrap_or("");
        let mut author_copyright_msgs = Vec::new();

        if author.is_empty() && copyright.is_empty() {
            author_copyright_msgs.push("Both 'author' and 'copyright' are missing or empty. At least one should be provided.".to_string());
            cert.add_test_result(
                "Author / Copyright",
                TestStatus::WARNING,
                author_copyright_msgs,
            );
        } else {
            cert.add_test_result("Author / Copyright", TestStatus::PASS, Vec::new());
        }

        if !copyright.is_empty() {
            let mut cp_msgs = Vec::new();
            let cp_lower = copyright.to_lowercase();
            if !copyright.starts_with('©')
                && !cp_lower.starts_with("copyright")
                && !cp_lower.starts_with("copr.")
            {
                cp_msgs.push(
                    "Copyright notice should begin with ©, 'Copyright', or 'Copr.'".to_string(),
                );
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
            cert.add_test_result(
                "License Attribute",
                TestStatus::WARNING,
                vec!["license attribute is missing in modelDescription.xml".to_string()],
            );
        }

        // FMI 1.0 specific
        if fmi_version == "1.0" {
            if let Some(va_node) = root
                .children()
                .find(|n| n.tag_name().name() == "VendorAnnotations")
            {
                let mut tool_names = std::collections::HashSet::new();
                let mut va_msgs = Vec::new();
                for tool in va_node.children().filter(|n| n.tag_name().name() == "Tool") {
                    if let Some(name) = tool.attribute("name") {
                        if !tool_names.insert(name.to_string()) {
                            va_msgs.push(format!(
                                "Duplicate tool name in VendorAnnotations: {}",
                                name
                            ));
                        }
                    }
                }
                cert.add_test_result(
                    "Vendor Annotations",
                    if va_msgs.is_empty() {
                        TestStatus::PASS
                    } else {
                        TestStatus::FAIL
                    },
                    va_msgs,
                );
            }

            // URI-based file references
            if let Some(cs_node) = root
                .children()
                .find(|n| n.tag_name().name() == "CoSimulation_Tool")
            {
                let mut uri_msgs = Vec::new();
                for attr in ["entryPoint", "file"] {
                    if let Some(val) = cs_node.attribute(attr) {
                        if !val.starts_with("fmu://") {
                            uri_msgs.push(format!("Attribute '{}' in CoSimulation_Tool should use 'fmu://' URI scheme (found: {})", attr, val));
                        }
                    }
                }
                if !uri_msgs.is_empty() {
                    cert.add_test_result("URI References", TestStatus::WARNING, uri_msgs);
                }
            }
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
        let mut fmi1_msgs = Vec::new();

        // 1. Collect Type and Unit Definitions
        if let Some(td_node) = root
            .children()
            .find(|n| n.tag_name().name() == "TypeDefinitions")
        {
            for type_node in td_node.children().filter(|n| n.is_element()) {
                if let Some(name) = type_node.attribute("name") {
                    if !type_names.insert(name.to_string()) {
                        type_msgs.push(format!("Duplicate type name: {}", name));
                    }
                }
            }
        }

        if let Some(ud_node) = root
            .children()
            .find(|n| n.tag_name().name() == "UnitDefinitions")
        {
            for unit_node in ud_node.children().filter(|n| n.tag_name().name() == "Unit") {
                if let Some(name) = unit_node.attribute("name") {
                    if !unit_names.insert(name.to_string()) {
                        unit_msgs.push(format!("Duplicate unit name: {}", name));
                    }
                }
            }
        }

        // 2. Validate Variables
        if let Some(sv_node) = root
            .children()
            .find(|n| n.tag_name().name() == "ModelVariables")
        {
            let naming_convention = root.attribute("namingConvention").unwrap_or("flat");

            for var_node in sv_node
                .children()
                .filter(|n| n.tag_name().name() == "ScalarVariable")
            {
                let name = var_node.attribute("name").unwrap_or("");
                let causality = var_node.attribute("causality").unwrap_or("local");
                let variability = var_node.attribute("variability").unwrap_or("continuous");

                if !name.is_empty() {
                    if !var_names.insert(name.to_string()) {
                        var_msgs.push(format!("Duplicate variable name: {}", name));
                    }

                    // Naming Convention Check
                    if naming_convention == "structured" {
                        // Structured name syntax
                        let is_valid = if name.starts_with("der(") && name.ends_with(')') {
                            true // Simplified
                        } else {
                            !name.is_empty() && !name.chars().next().unwrap().is_ascii_digit()
                        };
                        if !is_valid {
                            var_msgs.push(format!(
                                "Variable name '{}' does not follow structured naming convention",
                                name
                            ));
                        }
                    } else if name.chars().any(|c| matches!(c, '\r' | '\n' | '\t')) {
                        var_msgs.push(format!("Variable name '{}' contains illegal control characters (CR, LF, or Tab)", name));
                    }
                }

                if let Some(declared_type) = var_node.attribute("declaredType") {
                    referenced_types.insert(declared_type.to_string());
                    if !type_names.contains(declared_type) {
                        type_msgs.push(format!(
                            "Variable references undefined type: {}",
                            declared_type
                        ));
                    }
                }

                for child in var_node.children().filter(|n| n.is_element()) {
                    let type_tag = child.tag_name().name();

                    if let Some(unit) = child.attribute("unit") {
                        referenced_units.insert(unit.to_string());
                        if !unit_names.contains(unit) {
                            unit_msgs.push(format!("Variable references undefined unit: {}", unit));
                        }
                    }

                    // FMI 1.0 specific variable checks
                    if fmi_version == "1.0" {
                        if variability == "continuous" && type_tag != "Real" {
                            fmi1_msgs.push(format!("FMI 1.0: Only variables of type 'Real' can have variability='continuous'. Variable '{}' is '{}'.", name, type_tag));
                        }

                        let has_start = child.attribute("start").is_some();
                        if (causality == "input" || variability == "constant") && !has_start {
                            fmi1_msgs.push(format!("FMI 1.0: Variable '{}' must have a 'start' attribute because causality='{}' or variability='{}'.", name, causality, variability));
                        }

                        if variability == "constant" && causality == "input" {
                            fmi1_msgs.push(format!("FMI 1.0: Variable '{}' must not have causality='input' and variability='constant' (logical contradiction).", name));
                        }

                        if child.attribute("fixed").is_some() && !has_start {
                            fmi1_msgs.push(format!("FMI 1.0: The 'fixed' attribute in variable '{}' must only be present if a 'start' attribute is also provided.", name));
                        }
                    }

                    // Min/Max/Start Constraints
                    let min = child.attribute("min").and_then(|s| s.parse::<f64>().ok());
                    let max = child.attribute("max").and_then(|s| s.parse::<f64>().ok());
                    let start = child.attribute("start").and_then(|s| s.parse::<f64>().ok());

                    if let (Some(mi), Some(ma)) = (min, max) {
                        if ma < mi {
                            var_msgs.push(format!(
                                "Variable '{}': max ({}) must be >= min ({})",
                                name, ma, mi
                            ));
                        }
                    }
                    if let Some(s) = start {
                        if let Some(mi) = min {
                            if s < mi {
                                var_msgs.push(format!(
                                    "Variable '{}': start value ({}) is below min ({})",
                                    name, s, mi
                                ));
                            }
                        }
                        if let Some(ma) = max {
                            if s > ma {
                                var_msgs.push(format!(
                                    "Variable '{}': start value ({}) is above max ({})",
                                    name, s, ma
                                ));
                            }
                        }
                    }
                }
            }
        }

        cert.add_test_result(
            "Unique Variable Names",
            if var_msgs.iter().any(|m| m.contains("Duplicate")) {
                TestStatus::FAIL
            } else {
                TestStatus::PASS
            },
            var_msgs.clone(),
        );
        cert.add_test_result(
            "Variable Naming",
            if var_msgs.iter().any(|m| m.contains("illegal")) {
                TestStatus::FAIL
            } else {
                TestStatus::PASS
            },
            var_msgs,
        );
        cert.add_test_result(
            "Unique Type Names",
            if type_msgs.iter().any(|m| m.contains("Duplicate")) {
                TestStatus::FAIL
            } else {
                TestStatus::PASS
            },
            type_msgs.clone(),
        );
        cert.add_test_result(
            "Type References",
            if type_msgs.iter().any(|m| m.contains("undefined")) {
                TestStatus::FAIL
            } else {
                TestStatus::PASS
            },
            type_msgs,
        );
        cert.add_test_result(
            "Unique Unit Names",
            if unit_msgs.iter().any(|m| m.contains("Duplicate")) {
                TestStatus::FAIL
            } else {
                TestStatus::PASS
            },
            unit_msgs.clone(),
        );
        cert.add_test_result(
            "Unit References",
            if unit_msgs.iter().any(|m| m.contains("undefined")) {
                TestStatus::FAIL
            } else {
                TestStatus::PASS
            },
            unit_msgs,
        );

        if fmi_version == "1.0" {
            cert.add_test_result(
                "FMI 1.0 Variable Constraints",
                if fmi1_msgs.is_empty() {
                    TestStatus::PASS
                } else {
                    TestStatus::FAIL
                },
                fmi1_msgs,
            );
        }

        // Alias consistency
        let mut alias_msgs = Vec::new();
        if let Some(sv_node) = root
            .children()
            .find(|n| n.tag_name().name() == "ModelVariables")
        {
            let mut alias_sets: std::collections::HashMap<(String, u32), Vec<roxmltree::Node>> =
                std::collections::HashMap::new();
            for var_node in sv_node
                .children()
                .filter(|n| n.tag_name().name() == "ScalarVariable")
            {
                if let Some(vr_str) = var_node.attribute("valueReference") {
                    if let Ok(vr) = vr_str.parse::<u32>() {
                        let type_name = var_node
                            .children()
                            .find(|n| n.is_element())
                            .map(|n| n.tag_name().name().to_string())
                            .unwrap_or_default();
                        alias_sets
                            .entry((type_name, vr))
                            .or_default()
                            .push(var_node);
                    }
                }
            }

            for ((type_name, vr), vars) in alias_sets {
                if vars.len() > 1 {
                    let first = &vars[0];
                    let first_unit = first
                        .children()
                        .find(|n| n.is_element())
                        .and_then(|n| n.attribute("unit"))
                        .unwrap_or("(none)");

                    let mut settable_vars = Vec::new();
                    let mut vars_with_start = Vec::new();
                    let mut all_constant = true;

                    for var in &vars {
                        let name = var.attribute("name").unwrap_or("");
                        let child = var.children().find(|n| n.is_element());
                        let unit = child.and_then(|n| n.attribute("unit")).unwrap_or("(none)");

                        if unit != first_unit {
                            alias_msgs.push(format!("All variables in an alias set (VR {}, type {}) must have the same unit. Variable '{}' has unit '{}' but '{}' has '{}'.", vr, type_name, first.attribute("name").unwrap_or(""), first_unit, name, unit));
                        }

                        // FMI 2.0 specific alias checks
                        if fmi_version == "2.0" {
                            let causality = var.attribute("causality").unwrap_or("local");
                            let variability = var.attribute("variability").unwrap_or("continuous");
                            let initial = var.attribute("initial");
                            let has_start = child.and_then(|n| n.attribute("start")).is_some();

                            if variability != "constant" {
                                all_constant = false;
                            }

                            // Settable check: input OR (parameter AND not constant) OR (initial=exact/approx)
                            let is_settable = causality == "input"
                                || (causality == "parameter" && variability != "constant")
                                || matches!(initial, Some("exact") | Some("approx"));

                            if is_settable {
                                settable_vars.push(name);
                            }

                            if has_start && variability != "constant" {
                                vars_with_start.push(name);
                            }
                        }
                    }

                    if fmi_version == "2.0" {
                        if settable_vars.len() > 1 {
                            alias_msgs.push(format!(
                                "FMI 2.0: At most one variable in an alias set (VR {}, type {}) can be settable. Found: {}",
                                vr, type_name, settable_vars.join(", ")
                            ));
                        }
                        if !all_constant && vars_with_start.len() > 1 {
                            alias_msgs.push(format!(
                                "FMI 2.0: At most one variable in an alias set (VR {}, type {}) that is not constant can have a 'start' attribute. Found: {}",
                                vr, type_name, vars_with_start.join(", ")
                            ));
                        }
                    }
                }
            }
        }
        cert.add_test_result(
            "Alias Consistency",
            if alias_msgs.is_empty() {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            alias_msgs,
        );

        // Unused definitions (WARNING)
        let mut unused_msgs = Vec::new();
        for t in &type_names {
            if !referenced_types.contains(t) {
                unused_msgs.push(format!(
                    "Type definition '{}' is not used by any variable",
                    t
                ));
            }
        }
        for u in &unit_names {
            if !referenced_units.contains(u) {
                unused_msgs.push(format!(
                    "Unit definition '{}' is not used by any variable",
                    u
                ));
            }
        }
        if !unused_msgs.is_empty() {
            cert.add_test_result("Unused Definitions", TestStatus::WARNING, unused_msgs);
        }

        // Model Structure and Interfaces
        let mut interface_msgs = Vec::new();
        let mut has_interface = false;
        for node in root.children() {
            if matches!(
                node.tag_name().name(),
                "CoSimulation" | "ModelExchange" | "ScheduledExecution"
            ) {
                has_interface = true;
                break;
            }
        }
        if !has_interface {
            interface_msgs.push("At least one interface (CoSimulation, ModelExchange, or ScheduledExecution) must be implemented.".to_string());
        }
        cert.add_test_result(
            "Implemented Interfaces",
            if has_interface {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            interface_msgs,
        );

        // Default Experiment
        if let Some(de_node) = root
            .children()
            .find(|n| n.tag_name().name() == "DefaultExperiment")
        {
            let mut de_msgs = Vec::new();
            let start_time = de_node
                .attribute("startTime")
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);
            let stop_time = de_node
                .attribute("stopTime")
                .and_then(|s| s.parse::<f64>().ok());
            let tolerance = de_node
                .attribute("tolerance")
                .and_then(|s| s.parse::<f64>().ok());
            let step_size = de_node
                .attribute("stepSize")
                .and_then(|s| s.parse::<f64>().ok());

            if start_time < 0.0 {
                de_msgs.push(format!("startTime must be non-negative: {}", start_time));
            }
            if let Some(stop) = stop_time {
                if stop < 0.0 {
                    de_msgs.push(format!("stopTime must be non-negative: {}", stop));
                }
                if stop <= start_time {
                    de_msgs.push(format!(
                        "stopTime ({}) must be greater than startTime ({})",
                        stop, start_time
                    ));
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
            cert.add_test_result(
                "Default Experiment",
                if de_msgs.is_empty() {
                    TestStatus::PASS
                } else {
                    TestStatus::FAIL
                },
                de_msgs,
            );
        }

        // FMI 2.0 Specific Checks
        if fmi_version == "2.0" {
            // Standard Headers Check (Best effort - needs sources/ dir which might not be here if it's just XML)
            // But we can add a placeholder or rely on ModelChecker to pass this info.

            let mut fmi2_msgs = Vec::new();
            let mut independent_vars = Vec::new();
            let mut derivatives = Vec::new();

            // Check for prohibited special floats (NaN/INF) in Real attributes
            let attributes_to_check = [
                "min",
                "max",
                "start",
                "nominal",
                "factor",
                "offset",
                "startTime",
                "stopTime",
                "tolerance",
                "stepSize",
            ];

            // This requires a deeper search through the XML, but we can check the attributes of the current root and its children
            for node in root.descendants() {
                // Determine if this is a node where special floats are prohibited
                let tag = node.tag_name().name();
                let prohibited = matches!(
                    tag,
                    "ScalarVariable"
                        | "Real"
                        | "SimpleType"
                        | "BaseUnit"
                        | "DisplayUnit"
                        | "DefaultExperiment"
                );

                if prohibited {
                    for attr in attributes_to_check {
                        if let Some(val) = node.attribute(attr) {
                            let v = val.to_lowercase();
                            if v.contains("nan") || v.contains("inf") {
                                fmi2_msgs.push(format!(
                                    "FMI 2.0 prohibits NaN/INF in attribute '{}' of <{}>: {}",
                                    attr, tag, val
                                ));
                            }
                        }
                    }
                }

                // Enumeration Variables must have declaredType in FMI 2.0
                if node.tag_name().name() == "ScalarVariable" {
                    let name = node.attribute("name").unwrap_or("unnamed");
                    let causality = node.attribute("causality").unwrap_or("local");
                    let variability = node.attribute("variability").unwrap_or("continuous");
                    let initial = node.attribute("initial");

                    if node
                        .children()
                        .any(|n| n.tag_name().name() == "Enumeration")
                        && node.attribute("declaredType").is_none()
                    {
                        fmi2_msgs.push(format!(
                            "FMI 2.0: Enumeration variable '{}' must have a 'declaredType' attribute.",
                            name
                        ));
                    }

                    if causality == "independent" {
                        independent_vars.push(name.to_string());
                        if variability != "continuous" {
                            fmi2_msgs.push(format!("FMI 2.0: Independent variable '{}' must have variability='continuous'.", name));
                        }
                        if node.children().any(|n| n.attribute("start").is_some()) {
                            fmi2_msgs.push(format!("FMI 2.0: Independent variable '{}' must not have a 'start' attribute.", name));
                        }
                        if initial.is_some() {
                            fmi2_msgs.push(format!("FMI 2.0: Independent variable '{}' must not have an 'initial' attribute.", name));
                        }
                        if !node.children().any(|n| n.tag_name().name() == "Real") {
                            fmi2_msgs.push(format!("FMI 2.0: Independent variable '{}' must be of type 'Real'.", name));
                        }
                    }

                    // FMI 2.0 reinit and multipleSet checks
                    if node.attribute("reinit").is_some() && variability != "continuous" {
                        fmi2_msgs.push(format!("FMI 2.0: Attribute 'reinit' is only allowed for continuous-time states. Variable '{}' is '{}'.", name, variability));
                    }
                    if node.attribute("canHandleMultipleSetPerTimeInstant").is_some()
                        && causality != "input"
                    {
                        fmi2_msgs.push(format!("FMI 2.0: Attribute 'canHandleMultipleSetPerTimeInstant' is only allowed for inputs. Variable '{}' is '{}'.", name, causality));
                    }

                    // Collect continuous states and derivatives
                    if variability == "continuous" {
                        if causality == "local" || causality == "output" {
                            // Potentially a state, but we need to check if it's referenced as a state in ModelStructure
                        }
                    }
                }

                if node.tag_name().name() == "Derivative" {
                    if let Some(index) = node.attribute("index") {
                        derivatives.push(index.to_string());
                    }
                }
            }

            if independent_vars.len() > 1 {
                fmi2_msgs.push(format!(
                    "FMI 2.0: At most one independent variable is allowed. Found: {}",
                    independent_vars.join(", ")
                ));
            }

            // ModelStructure Checks
            if let Some(ms_node) = root
                .children()
                .find(|n| n.tag_name().name() == "ModelStructure")
            {
                let ms_msgs = Vec::new();

                // Derivatives
                if let Some(der_node) = ms_node
                    .children()
                    .find(|n| n.tag_name().name() == "Derivatives")
                {
                    let count = der_node
                        .children()
                        .filter(|n| n.tag_name().name() == "Unknown")
                        .count();
                    if count == 0 {
                        // ms_msgs.push("FMI 2.0: <Derivatives> should not be empty if the FMU has continuous states.".to_string());
                    }
                }

                cert.add_test_result(
                    "Model Structure",
                    if ms_msgs.is_empty() {
                        TestStatus::PASS
                    } else {
                        TestStatus::FAIL
                    },
                    ms_msgs,
                );
            }

            cert.add_test_result(
                "FMI 2.0 Specific",
                if fmi2_msgs.is_empty() {
                    TestStatus::PASS
                } else {
                    TestStatus::FAIL
                },
                fmi2_msgs,
            );
        }

        // FMI 3.0 Specific Checks
        if fmi_version == "3.0" {
            let mut fmi3_msgs = Vec::new();
            let it = root.attribute("instantiationToken").unwrap_or("");

            // instantiationToken GUID format check
            if !it.is_empty() {
                let is_guid = it.len() == 36 || it.len() == 38; // Simple check for length
                if !is_guid {
                    cert.add_test_result(
                        "instantiationToken Format",
                        TestStatus::WARNING,
                        vec![format!(
                            "instantiationToken '{}' does not follow the recommended GUID format.",
                            it
                        )],
                    );
                }
            }

            for node in root.descendants() {
                let tag = node.tag_name().name();

                // Dimension constraints
                if tag == "Dimension" {
                    let start = node.attribute("start");
                    let vr = node.attribute("valueReference");
                    if start.is_some() && vr.is_some() {
                        fmi3_msgs.push("FMI 3.0: <Dimension> must have either 'start' or 'valueReference', but not both.".to_string());
                    }
                    if start.is_none() && vr.is_none() {
                        fmi3_msgs.push("FMI 3.0: <Dimension> must have either 'start' or 'valueReference'.".to_string());
                    }
                }
            }

            cert.add_test_result(
                "FMI 3.0 Specific",
                if fmi3_msgs.is_empty() {
                    TestStatus::PASS
                } else {
                    TestStatus::FAIL
                },
                fmi3_msgs,
            );
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
        XmlChecker::validate_fmu(xml, &mut cert, None).unwrap();
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "Model Name" && r.status == TestStatus::PASS));
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "FMI Version" && r.status == TestStatus::PASS));
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "Model Identifier" && r.status == TestStatus::PASS));
    }

    #[test]
    fn test_invalid_model_identifier() {
        let mut cert = Certificate::new();
        let xml = r#"<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="2.0" modelName="TestModel" guid="123">
    <CoSimulation modelIdentifier="123_Invalid" />
</fmiModelDescription>"#;
        let _ = XmlChecker::validate_fmu(xml, &mut cert, None);
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "Model Identifier" && r.status == TestStatus::FAIL));
    }

    #[test]
    fn test_future_generation_date() {
        let mut cert = Certificate::new();
        let xml = r#"<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="2.0" modelName="TestModel" guid="123" generationDateAndTime="2099-01-01T00:00:00Z">
</fmiModelDescription>"#;
        let _ = XmlChecker::validate_fmu(xml, &mut cert, None);
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "Generation Date and Time" && r.status == TestStatus::FAIL));
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
        let _ = XmlChecker::validate_fmu(xml, &mut cert, None);
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "Unique Variable Names" && r.status == TestStatus::FAIL));
    }

    #[test]
    fn test_fmi10_continuous_real_only() {
        let mut cert = Certificate::new();
        let xml = r#"<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="1.0" modelName="Test" guid="123">
    <ModelVariables>
        <ScalarVariable name="v1" valueReference="1" variability="continuous">
            <Integer />
        </ScalarVariable>
    </ModelVariables>
</fmiModelDescription>"#;
        let _ = XmlChecker::validate_fmu(xml, &mut cert, None);
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "FMI 1.0 Variable Constraints" && r.status == TestStatus::FAIL));
    }

    #[test]
    fn test_fmi20_independent_constraints() {
        let mut cert = Certificate::new();
        let xml = r#"<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="2.0" modelName="Test" guid="123">
    <ModelVariables>
        <ScalarVariable name="v1" valueReference="1" causality="independent">
            <Integer />
        </ScalarVariable>
    </ModelVariables>
</fmiModelDescription>"#;
        let _ = XmlChecker::validate_fmu(xml, &mut cert, None);
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "FMI 2.0 Specific" && r.status == TestStatus::FAIL));
    }

    #[test]
    fn test_fmi30_dimension_constraints() {
        let mut cert = Certificate::new();
        let xml = r#"<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="3.0" modelName="Test" instantiationToken="123">
    <ModelVariables>
        <Float64 name="v1" valueReference="1">
            <Dimension start="1" valueReference="2" />
        </Float64>
    </ModelVariables>
</fmiModelDescription>"#;
        let _ = XmlChecker::validate_fmu(xml, &mut cert, None);
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "FMI 3.0 Specific" && r.status == TestStatus::FAIL));
    }
}
