use crate::archive::ArchiveChecker;
use crate::binary_parser::BinaryParser;
use crate::certificate::{Certificate, TestStatus};
use crate::checker::xml_checker::XmlChecker;
use std::fs;
use std::io::Read;
use std::path::Path;
use zip::ZipArchive;

pub struct ModelChecker {
    pub cert: Certificate,
}

impl ModelChecker {
    pub fn new() -> Self {
        Self {
            cert: Certificate::new(),
        }
    }

    pub fn validate<P: AsRef<Path>>(&mut self, path: P) -> anyhow::Result<()> {
        let path_ref = path.as_ref();

        if !path_ref.exists() {
            self.cert.add_test_result(
                "File Existence",
                TestStatus::FAIL,
                vec![format!("Path does not exist: {:?}", path_ref)],
            );
            return Err(anyhow::anyhow!("Path does not exist"));
        }

        let is_dir = path_ref.is_dir();
        let mut model_type = "UNKNOWN";

        if is_dir {
            if path_ref.join("modelDescription.xml").exists() {
                model_type = "FMU";
            } else if path_ref.join("SystemStructure.ssd").exists() {
                model_type = "SSP";
            }
        } else {
            let ext = path_ref.extension().and_then(|e| e.to_str()).unwrap_or("");
            if ext == "fmu" {
                model_type = "FMU";
            } else if ext == "ssp" {
                model_type = "SSP";
            }
        }

        if model_type == "UNKNOWN" {
            self.cert.add_test_result(
                "Model Identification",
                TestStatus::FAIL,
                vec!["Could not identify model type (FMU or SSP)".to_string()],
            );
            return Err(anyhow::anyhow!("Unknown model type"));
        }

        self.cert
            .add_test_result("Model Identification", TestStatus::PASS, Vec::new());
        self.cert
            .log(&format!("Identified model type: {}", model_type));

        if !is_dir {
            ArchiveChecker::validate(path_ref, &mut self.cert)?;
        }

        let mut xml_content = String::new();
        let mut temp_dir: Option<tempfile::TempDir> = None;

        if is_dir {
            let xml_path = if model_type == "FMU" {
                path_ref.join("modelDescription.xml")
            } else {
                path_ref.join("SystemStructure.ssd")
            };
            xml_content = fs::read_to_string(xml_path)?;
        } else {
            let file = fs::File::open(path_ref)?;
            let mut archive = ZipArchive::new(file)?;
            let xml_file_name = if model_type == "FMU" {
                "modelDescription.xml"
            } else {
                "SystemStructure.ssd"
            };
            let mut file = archive.by_name(xml_file_name)?;
            file.read_to_string(&mut xml_content)?;

            // For binary validation, we need to extract files to a temporary directory if it's an archive
            let td = tempfile::tempdir()?;
            let mut archive = ZipArchive::new(fs::File::open(path_ref)?)?;
            archive.extract(td.path())?;
            temp_dir = Some(td);
        }

        if model_type == "FMU" {
            XmlChecker::validate_fmu(&xml_content, &mut self.cert)?;

            // Binary Validation
            self.cert.log("\n--- BINARY VALIDATION ---");
            let base_path = if is_dir {
                path_ref.to_path_buf()
            } else {
                temp_dir.as_ref().unwrap().path().to_path_buf()
            };

            // Directory Structure Checks
            let mut dir_msgs = Vec::new();

            // Documentation
            let doc_dir = base_path.join("documentation");
            if !doc_dir.exists() {
                dir_msgs
                    .push("The 'documentation/' directory is missing (recommended).".to_string());
                self.cert.add_test_result(
                    "Documentation",
                    TestStatus::WARNING,
                    vec!["documentation/ directory is missing".to_string()],
                );
            } else {
                let mut doc_entry_point = false;
                if let Ok(entries) = fs::read_dir(&doc_dir) {
                    let mut count = 0;
                    for entry in entries.flatten() {
                        count += 1;
                        let name = entry.file_name();
                        let name_str = name.to_string_lossy();
                        if name_str == "index.html" || name_str == "_main.html" {
                            doc_entry_point = true;
                        }
                    }
                    if count == 0 {
                        dir_msgs.push(
                            "The 'documentation/' directory is effectively empty.".to_string(),
                        );
                    }
                }
                if !doc_entry_point {
                    dir_msgs.push(
                        "Standard documentation entry point (index.html or _main.html) is missing."
                            .to_string(),
                    );
                }
                self.cert.add_test_result(
                    "Documentation",
                    if doc_entry_point {
                        TestStatus::PASS
                    } else {
                        TestStatus::FAIL
                    },
                    dir_msgs.clone(),
                );
            }

            // Licenses
            let license_dir = base_path.join("documentation").join("licenses");
            if !license_dir.exists() {
                self.cert.add_test_result(
                    "Licenses",
                    TestStatus::WARNING,
                    vec!["documentation/licenses/ directory is missing".to_string()],
                );
            } else {
                let mut license_entry_point = false;
                if let Ok(entries) = fs::read_dir(&license_dir) {
                    for entry in entries.flatten() {
                        let name = entry.file_name();
                        let name_str = name.to_string_lossy();
                        if name_str == "license.txt"
                            || name_str == "license.html"
                            || name_str == "license.spdx"
                        {
                            license_entry_point = true;
                        }
                    }
                }
                self.cert.add_test_result("Licenses", if license_entry_point { TestStatus::PASS } else { TestStatus::FAIL }, if license_entry_point { Vec::new() } else { vec!["Standard license entry point (license.txt, license.html, or license.spdx) is missing in documentation/licenses/.".to_string()] });
            }

            let binaries_dir = base_path.join("binaries");
            if binaries_dir.exists() && binaries_dir.is_dir() {
                let mut binary_msgs = Vec::new();
                for entry in fs::read_dir(binaries_dir)? {
                    let entry = entry?;
                    let platform_path = entry.path();
                    if platform_path.is_dir() {
                        for bin_entry in fs::read_dir(&platform_path)? {
                            let bin_entry = bin_entry?;
                            let bin_path = bin_entry.path();
                            if bin_path.is_file() {
                                match BinaryParser::parse(&bin_path) {
                                    Ok(info) => {
                                        if !info.is_shared_library {
                                            binary_msgs.push(format!(
                                                "Binary is not a shared library: {:?}",
                                                bin_path.file_name().unwrap()
                                            ));
                                        }

                                        let platform = platform_path
                                            .file_name()
                                            .and_then(|n| n.to_str())
                                            .unwrap_or("");
                                        let mut arch_match = false;
                                        for arch in &info.architectures {
                                            if platform.contains(&arch.architecture) {
                                                arch_match = true;
                                                break;
                                            }
                                        }
                                        if !arch_match {
                                            binary_msgs.push(format!("Binary '{:?}' does not match platform identifier '{}'", bin_path.file_name().unwrap(), platform));
                                        }

                                        self.cert.log(&format!(
                                            "Validated binary: {:?} ({:?})",
                                            bin_path.file_name().unwrap(),
                                            info.format
                                        ));
                                    }
                                    Err(e) => {
                                        binary_msgs.push(format!(
                                            "Failed to parse binary {:?}: {}",
                                            bin_path.file_name().unwrap(),
                                            e
                                        ));
                                    }
                                }
                            }
                        }
                    }
                }
                self.cert.add_test_result(
                    "Binary Exports",
                    if binary_msgs.is_empty() {
                        TestStatus::PASS
                    } else {
                        TestStatus::FAIL
                    },
                    binary_msgs,
                );
            }
        } else if model_type == "SSP" {
            XmlChecker::validate_ssp(&xml_content, &mut self.cert)?;
        }

        let mut summary = self.cert.summary.clone();
        summary.standard = model_type.to_string();
        self.cert.set_summary(summary);

        Ok(())
    }
}

impl Default for ModelChecker {
    fn default() -> Self {
        Self::new()
    }
}
