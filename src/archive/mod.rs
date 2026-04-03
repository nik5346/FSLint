use crate::certificate::{Certificate, TestStatus};
use std::collections::{HashMap, HashSet};
use std::fs::File;
use std::path::Path;
use zip::read::HasZipMetadata;
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
        let mut format_msgs = Vec::new();
        let mut integrity_msgs = Vec::new();
        let mut security_msgs = Vec::new();

        // Disk Spanning and Multi-disk archives
        // zip-rs doesn't support multi-disk archives for reading.
        // But if it is multi-disk, ZipArchive::new usually fails.
        // We can't easily check for disk spanning with the high-level API.

        // [SECURITY] Overlapping File Entries
        // We'll track byte ranges of local file headers and data.
        let mut regions: Vec<(u64, u64)> = Vec::new();

        for i in 0..archive.len() {
            let file = archive.by_index(i)?;
            let name = file.name().to_string();

            // Archive Format Checks
            if file.is_dir() && !name.ends_with('/') {
                format_msgs.push(format!("Directory entry must end with '/': {}", name));
            }

            if name.starts_with("./") {
                 format_msgs.push(format!("Leading './' in path should be avoided: {}", name));
            }
            if name.contains("//") {
                 format_msgs.push(format!("Multiple consecutive slashes '//' in path should be avoided: {}", name));
            }

            // [SECURITY] Symbolic Links
            // Note: zip-rs doesn't directly tell if it's a symlink in a cross-platform way easily,
            // but we can check the unix mode if available.
            if let Some(mode) = file.unix_mode() {
                if (mode & 0o120000) == 0o120000 {
                    security_msgs.push(format!("Symbolic links are not allowed: {}", name));
                    fatal_error = true;
                }
            }

            // [SECURITY] Encryption
            if file.encrypted() {
                format_msgs.push(format!("Encrypted files are not allowed: {}", name));
                fatal_error = true;
            }

            // Version Needed to Extract
            let version_needed = file.get_metadata().version_needed();
            if version_needed > 20 {
                // 2.0 is 20 in the ZIP spec
                format_msgs.push(format!(
                    "Version needed to extract is too high: {} (must be <= 2.0)",
                    version_needed as f32 / 10.0
                ));
            }

            // Data Descriptor Check
            if file.get_metadata().using_data_descriptor && file.compression() != zip::CompressionMethod::Deflated {
                integrity_msgs.push(format!("Data descriptor is only allowed with 'deflate' compression: {}", name));
            }

            // [SECURITY] Overlapping File Entries (simplified check)
            let start = file.header_start();
            let end = file.data_start() + file.compressed_size();
            for (r_start, r_end) in &regions {
                if (start >= *r_start && start < *r_end) || (end > *r_start && end <= *r_end) {
                    format_msgs.push(format!(
                        "[SECURITY] Overlapping file entries detected: {} overlaps with a previous entry",
                        name
                    ));
                    fatal_error = true;
                    break;
                }
            }
            regions.push((start, end));

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
            // zip-rs handles Bit 11 (UTF-8 encoding) automatically.
            // We'll check if the filename was decoded using Bit 11.
            let is_ascii = name.is_ascii();
            // Bit 11 Check
            // Bit 11 is not directly exposed as flags() in zip-rs 2.4.2 ZipFile.
            // But ZipFileData has is_utf8 field.
            let bit11_set = file.get_metadata().is_utf8;
            if !is_ascii {
                 bit11_msgs.push(format!("Path contains non-ASCII characters (recommended to avoid for portability): {}", name));
            }

            if !is_ascii && !bit11_set {
                bit11_msgs.push(format!(
                    "Language encoding flag (bit 11) MUST be set for '{}' because it contains non-ASCII characters.",
                    name
                ));
            } else if is_ascii && bit11_set {
                bit11_msgs.push(format!(
                    "Language encoding flag (bit 11) SHOULD be 0 for '{}' because it contains only ASCII characters.",
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
            "Archive Format",
            if format_msgs.is_empty() {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            format_msgs,
        );
        cert.add_test_result(
            "Central Directory Integrity",
            if integrity_msgs.is_empty() {
                TestStatus::PASS
            } else {
                TestStatus::FAIL
            },
            integrity_msgs,
        );

        cert.add_test_result(
            "Language Encoding Flag",
            if bit11_msgs.is_empty() {
                TestStatus::PASS
            } else if bit11_msgs.iter().any(|m| m.contains("MUST")) {
                TestStatus::FAIL
            } else {
                TestStatus::WARNING
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

        cert.add_test_result(
            "Symbolic Links",
            if security_msgs.iter().any(|m| m.contains("Symbolic links")) {
                TestStatus::FAIL
            } else {
                TestStatus::PASS
            },
            security_msgs.iter().filter(|m| m.contains("Symbolic links")).cloned().collect(),
        );

        if fatal_error {
            return Err(anyhow::anyhow!("Critical archive validation failure"));
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::certificate::Certificate;
    use tempfile::NamedTempFile;
    use zip::write::FileOptions;

    #[test]
    fn test_valid_fmu_extension() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("fmu");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("modelDescription.xml", FileOptions::default())
            .unwrap();
        zip.finish().unwrap();

        ArchiveChecker::validate(&path, &mut cert).unwrap();
        assert!(cert.results.iter().any(|r| r.test_name == "File Extension" && r.status == TestStatus::PASS));
    }

    #[test]
    fn test_invalid_extension() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("txt");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("test.txt", FileOptions::default()).unwrap();
        zip.finish().unwrap();

        let _ = ArchiveChecker::validate(&path, &mut cert);
        assert!(cert.results.iter().any(|r| r.test_name == "File Extension" && r.status == TestStatus::FAIL));
    }

    #[test]
    fn test_zip_slip() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("fmu");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("../outside.txt", FileOptions::default())
            .unwrap();
        zip.finish().unwrap();

        let result = ArchiveChecker::validate(&path, &mut cert);
        assert!(result.is_err());
        assert!(cert.results.iter().any(|r| r.test_name == "Zip Slip Check" && r.status == TestStatus::FAIL));
    }
}
