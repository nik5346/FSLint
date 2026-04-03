use crate::certificate::{Certificate, TestStatus};
use roxmltree::Document;

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
