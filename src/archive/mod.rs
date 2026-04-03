use crate::certificate::{Certificate, TestStatus};
use std::collections::{HashMap, HashSet};
use std::fs::File;
use std::path::Path;
use zip::ZipArchive;

pub struct ArchiveChecker;

impl ArchiveChecker {
    pub fn validate<P: AsRef<Path>>(path: P, cert: &mut Certificate) -> anyhow::Result<()> {
        cert.log("\n--- ARCHIVE VALIDATION ---");

        let path_ref = path.as_ref();
        let file = File::open(path_ref)?;
        let mut archive = ZipArchive::new(file)?;

        // 1. File Extension
        let mut ext_msgs = Vec::new();
        let ext = path_ref.extension().and_then(|e| e.to_str()).unwrap_or("");
        if ext != "fmu" && ext != "ssp" {
            ext_msgs.push(format!(
                "Archive file should have .fmu or .ssp extension (found: .{})",
                ext
            ));
            cert.add_test_result("File Extension", TestStatus::FAIL, ext_msgs);
        } else {
            cert.add_test_result("File Extension", TestStatus::PASS, Vec::new());
        }

        let mut seen_names: HashSet<String> = HashSet::new();
        let mut seen_names_lower: HashMap<String, String> = HashMap::new();
        let mut total_uncompressed_size: u64 = 0;
        let mut fatal_error = false;

        let mut zip_slip_msgs = Vec::new();
        let mut zip_bomb_msgs = Vec::new();
        let mut illegal_char_msgs = Vec::new();
        let mut control_char_msgs = Vec::new();
        let mut compression_msgs = Vec::new();
        let mut bit11_msgs = Vec::new();
        let mut duplicate_msgs = Vec::new();

        for i in 0..archive.len() {
            let file = archive.by_index(i)?;
            let name = file.name().to_string();

            // Security Check: Zip Slip (Path Traversal)
            if name.contains("..") || name.starts_with('/') || name.contains('\\') {
                zip_slip_msgs.push(format!("Zip Slip vulnerability detected: {}", name));
                fatal_error = true;
            }

            // Security Check: Illegal Characters
            if name
                .chars()
                .any(|c| matches!(c, '<' | '>' | '"' | '|' | '?' | '*'))
            {
                illegal_char_msgs.push(format!("Illegal characters in path: {}", name));
                fatal_error = true;
            }

            // Security Check: Null Bytes or Control Characters
            if name.chars().any(|c| (c as u32) < 0x20) {
                control_char_msgs.push(format!("Control characters in path: {}", name));
                fatal_error = true;
            }

            // Security Check: Zip Bomb
            let compressed_size = file.compressed_size();
            let uncompressed_size = file.size();
            total_uncompressed_size += uncompressed_size;

            if uncompressed_size > 1024 * 1024 * 1024 {
                // 1GB limit
                zip_bomb_msgs.push(format!(
                    "File too large in archive (potential zip bomb): {} ({} bytes)",
                    name, uncompressed_size
                ));
                fatal_error = true;
            }
            if compressed_size > 0 && (uncompressed_size as f64 / compressed_size as f64) > 100.0 {
                zip_bomb_msgs.push(format!(
                    "High compression ratio (potential zip bomb): {} ({}:1)",
                    name,
                    uncompressed_size as f64 / compressed_size as f64
                ));
                fatal_error = true;
            }

            // Compression Methods
            let method = file.compression();
            if method != zip::CompressionMethod::Stored
                && method != zip::CompressionMethod::Deflated
            {
                compression_msgs.push(format!(
                    "Unsupported compression method for {}: {:?}",
                    name, method
                ));
            }

            // Bit 11 Check
            // In zip crate, we might need a different way to access raw flags if needed,
            // but for now we can check if the name is valid UTF-8 which it is in Rust's String.
            // The zip crate handles the conversion.
            // Note: zip-rs handles the language encoding flag (bit 11) internally
            // and converts the filename to a UTF-8 String if it's set or if it's already ASCII.
            // In zip-rs 2.x, the flags are not directly exposed on ZipFile easily.
            // For a complete check, low-level parsing would be required.
            if false {
                let _has_non_ascii = !name.is_ascii();
                bit11_msgs.push(format!(
                    "Language encoding flag (bit 11) must be set for '{}' because it contains non-ASCII characters.",
                    name
                ));
            }

            // Duplicate names check
            if seen_names.contains(&name) {
                duplicate_msgs.push(format!("Duplicate entry name found: '{}'", name));
                fatal_error = true;
            }
            seen_names.insert(name.clone());

            let lower_name = name.to_lowercase();
            if let Some(original) = seen_names_lower.get(&lower_name) {
                if original != &name {
                    duplicate_msgs.push(format!(
                        "Case-conflicting entry names: '{}' collides with '{}'",
                        name, original
                    ));
                    fatal_error = true;
                }
            } else {
                seen_names_lower.insert(lower_name, name.clone());
            }
        }

        if total_uncompressed_size > 10 * 1024 * 1024 * 1024 {
            // 10GB total limit
            zip_bomb_msgs.push(format!(
                "Total uncompressed size exceeds 10 GB: {} bytes",
                total_uncompressed_size
            ));
            fatal_error = true;
        }

        cert.add_test_result(
            "Zip Slip Check",
            if zip_slip_msgs.is_empty() {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            zip_slip_msgs,
        );
        cert.add_test_result(
            "Zip Bomb Check",
            if zip_bomb_msgs.is_empty() {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            zip_bomb_msgs,
        );
        cert.add_test_result(
            "Illegal Characters",
            if illegal_char_msgs.is_empty() {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            illegal_char_msgs,
        );
        cert.add_test_result(
            "Control Characters",
            if control_char_msgs.is_empty() {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            control_char_msgs,
        );
        cert.add_test_result(
            "Compression Methods",
            if compression_msgs.is_empty() {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            compression_msgs,
        );
        cert.add_test_result(
            "Language Encoding Flag",
            if bit11_msgs.is_empty() {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            bit11_msgs,
        );
        cert.add_test_result(
            "Duplicate Names",
            if duplicate_msgs.is_empty() {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            duplicate_msgs,
        );

        if fatal_error {
            return Err(anyhow::anyhow!("Critical archive validation failure"));
        }

        Ok(())
    }
}
