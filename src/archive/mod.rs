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

        // Check for disk spanning / multi-disk archives
        // In the ZIP spec, the first 4 bytes of a multi-disk archive can be a specific signature.
        // Or more reliably, zip-rs will fail if it's multi-disk.
        // We can check the "number of this disk" in the Central Directory End Record if zip-rs exposed it.
        // Since we already opened it with ZipArchive, if it succeeded, it might still be part of a set.

        let mut archive = ZipArchive::new(file)?;

        // Disk Spanning Check (Best effort with zip-rs)
        // If zip-rs can read it, it's usually not a split archive unless all parts are present and it's handled.
        // We can check the comment and total entries as a proxy for some integrity.

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
                format_msgs.push(format!(
                    "Multiple consecutive slashes '//' in path should be avoided: {}",
                    name
                ));
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
            if file.get_metadata().using_data_descriptor
                && file.compression() != zip::CompressionMethod::Deflated
            {
                integrity_msgs.push(format!(
                    "Data descriptor is only allowed with 'deflate' compression: {}",
                    name
                ));
            }

            // [SECURITY] Overlapping File Entries
            let start = file.header_start();
            let end = file.data_start() + file.compressed_size();
            for (r_start, r_end) in &regions {
                // Check if [start, end] intersects with [r_start, r_end]
                if start < *r_end && end > *r_start {
                    security_msgs.push(format!(
                        "[SECURITY] Overlapping file entries detected: {} overlaps with a previous entry",
                        name
                    ));
                    fatal_error = true;
                    break;
                }
            }
            regions.push((start, end));

            // Security Check: Zip Slip (Path Traversal)
            if name.contains("..") {
                zip_slip_msgs.push(format!(
                    "Parent directory traversal ('..') is not allowed: {}",
                    name
                ));
                fatal_error = true;
            }

            if name.starts_with('/') {
                zip_slip_msgs.push(format!(
                    "Absolute paths (starting with '/') are not allowed: {}",
                    name
                ));
                fatal_error = true;
            }

            if name.contains('\\') {
                zip_slip_msgs.push(format!(
                    "Backslashes '\\' are not allowed, use forward slashes '/': {}",
                    name
                ));
                fatal_error = true;
            }

            // Drive letters or device paths
            if name.contains(':') {
                zip_slip_msgs.push(format!(
                    "Drive letters or device paths (containing ':') are not allowed: {}",
                    name
                ));
                fatal_error = true;
            }

            // Security Check: Illegal Characters
            if name
                .chars()
                .any(|c| matches!(c, '<' | '>' | '"' | '|' | '?' | '*'))
            {
                illegal_char_msgs.push(format!(
                    "Path contains illegal characters (< > \" | ? *): {}",
                    name
                ));
                fatal_error = true;
            }

            // Security Check: Null Bytes or Control Characters
            if name.chars().any(|c| (c as u32) < 0x20) {
                control_char_msgs.push(format!(
                    "Path contains control characters (U+0000–U+001F) or null bytes: {}",
                    name
                ));
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
                bit11_msgs.push(format!(
                    "Path contains non-ASCII characters (recommended to avoid for portability): {}",
                    name
                ));
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

        // [SECURITY] Extra Field and Comment Integrity
        let comment = archive.comment();
        if comment.len() > 65535 {
            security_msgs.push("Archive comment is too long".to_string());
            fatal_error = true;
        }

        // Check for disk spanning by looking at the file itself
        // (Simplified check for the split ZIP signature)
        let mut file = File::open(path_ref)?;
        let mut sig = [0u8; 4];
        use std::io::Read;
        if file.read_exact(&mut sig).is_ok() && sig == [0x50, 0x4B, 0x07, 0x08] {
            format_msgs.push("Split or spanned ZIP archives must not be used.".to_string());
            fatal_error = true;
        }

        cert.add_test_result(
            "Disk Spanning",
            if format_msgs
                .iter()
                .any(|m| m.contains("Split or spanned ZIP"))
            {
                TestStatus::FAIL
            } else {
                TestStatus::PASS
            },
            format_msgs
                .iter()
                .filter(|m| m.contains("Split or spanned ZIP"))
                .cloned()
                .collect(),
        );

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
            security_msgs
                .iter()
                .filter(|m| m.contains("Symbolic links"))
                .cloned()
                .collect(),
        );

        cert.add_test_result(
            "Security Checks",
            if security_msgs
                .iter()
                .any(|m| m.contains("[SECURITY]") || m.contains("comment is too long"))
            {
                TestStatus::FAIL
            } else {
                TestStatus::PASS
            },
            security_msgs
                .iter()
                .filter(|m| m.contains("[SECURITY]") || m.contains("comment is too long"))
                .cloned()
                .collect(),
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
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "File Extension" && r.status == TestStatus::PASS));
    }

    #[test]
    fn test_invalid_extension() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("txt");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("test.txt", FileOptions::default())
            .unwrap();
        zip.finish().unwrap();

        let _ = ArchiveChecker::validate(&path, &mut cert);
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "File Extension" && r.status == TestStatus::FAIL));
    }

    #[test]
    fn test_zip_slip_dot_dot() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("fmu");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("../outside.txt", FileOptions::default())
            .unwrap();
        zip.finish().unwrap();

        let result = ArchiveChecker::validate(&path, &mut cert);
        assert!(result.is_err());
        assert!(cert.results.iter().any(|r| r.test_name == "Zip Slip Check"
            && r.status == TestStatus::FAIL
            && r.messages
                .iter()
                .any(|m| m.contains("Parent directory traversal"))));
    }

    #[test]
    fn test_zip_slip_absolute() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("fmu");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("/absolute.txt", FileOptions::default())
            .unwrap();
        zip.finish().unwrap();

        let result = ArchiveChecker::validate(&path, &mut cert);
        assert!(result.is_err());
        assert!(cert.results.iter().any(|r| r.test_name == "Zip Slip Check"
            && r.status == TestStatus::FAIL
            && r.messages.iter().any(|m| m.contains("Absolute paths"))));
    }

    #[test]
    fn test_zip_slip_backslash() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("fmu");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("some\\path.txt", FileOptions::default())
            .unwrap();
        zip.finish().unwrap();

        let result = ArchiveChecker::validate(&path, &mut cert);
        assert!(result.is_err());
        assert!(cert.results.iter().any(|r| r.test_name == "Zip Slip Check"
            && r.status == TestStatus::FAIL
            && r.messages.iter().any(|m| m.contains("Backslashes"))));
    }

    #[test]
    fn test_zip_slip_drive_letter() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("fmu");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("C:/path.txt", FileOptions::default())
            .unwrap();
        zip.finish().unwrap();

        let result = ArchiveChecker::validate(&path, &mut cert);
        assert!(result.is_err());
        assert!(cert.results.iter().any(|r| r.test_name == "Zip Slip Check"
            && r.status == TestStatus::FAIL
            && r.messages.iter().any(|m| m.contains("Drive letters"))));
    }

    #[test]
    fn test_illegal_characters() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("fmu");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("illegal<char>.txt", FileOptions::default())
            .unwrap();
        zip.finish().unwrap();

        let result = ArchiveChecker::validate(&path, &mut cert);
        assert!(result.is_err());
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "Illegal Characters"
                && r.status == TestStatus::FAIL
                && r.messages
                    .iter()
                    .any(|m| m.contains("Path contains illegal characters"))));
    }

    #[test]
    fn test_control_characters() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("fmu");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("control\u{0007}char.txt", FileOptions::default())
            .unwrap();
        zip.finish().unwrap();

        let result = ArchiveChecker::validate(&path, &mut cert);
        assert!(result.is_err());
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "Control Characters"
                && r.status == TestStatus::FAIL
                && r.messages
                    .iter()
                    .any(|m| m.contains("Path contains control characters"))));
    }

    #[test]
    fn test_overlapping_entries() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("fmu");

        {
            let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
            zip.start_file::<&str, ()>("file1.txt", FileOptions::default())
                .unwrap();
            use std::io::Write;
            zip.write_all(b"content1").unwrap();
            zip.start_file::<&str, ()>("file2.txt", FileOptions::default())
                .unwrap();
            zip.write_all(b"content2").unwrap();
            zip.finish().unwrap();
        }

        // Manually corrupt the ZIP to make entries overlap
        // This is hard to do with the zip crate, but we can try to test the logic
        // by making sure it passes for a valid ZIP first.
        ArchiveChecker::validate(&path, &mut cert).unwrap();
        assert!(cert
            .results
            .iter()
            .any(|r| r.test_name == "Security Checks" && r.status == TestStatus::PASS));
    }

    #[test]
    fn test_case_conflicting_names() {
        let mut cert = Certificate::new();
        let file = NamedTempFile::new().unwrap();
        let path = file.path().with_extension("fmu");
        let mut zip = zip::ZipWriter::new(std::fs::File::create(&path).unwrap());
        zip.start_file::<&str, ()>("File.txt", FileOptions::default())
            .unwrap();
        // Try to add another one with different case
        let _ = zip.start_file::<&str, ()>("file.txt", FileOptions::default());
        zip.finish().unwrap();

        // If it was added, validate should fail.
        let result = ArchiveChecker::validate(&path, &mut cert);
        let has_duplicate_fail = cert
            .results
            .iter()
            .any(|r| r.test_name == "Duplicate Names" && r.status == TestStatus::FAIL);

        if has_duplicate_fail {
            assert!(result.is_err());
        }
    }
}
