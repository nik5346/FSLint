use std::fs::File;
use std::io::{Read, Seek};
use std::path::{Path, PathBuf};
use zip::ZipArchive;

pub struct ArchiveChecker;

impl ArchiveChecker {
    pub fn validate<P: AsRef<Path>>(path: P) -> anyhow::Result<()> {
        let file = File::open(path)?;
        let mut archive = ZipArchive::new(file)?;

        for i in 0..archive.len() {
            let file = archive.by_index(i)?;
            let name = file.name();

            // Security Check: Zip Slip (Path Traversal)
            if name.contains("..") || name.starts_with('/') || name.contains('\\') {
                return Err(anyhow::anyhow!("Zip Slip vulnerability detected: {}", name));
            }

            // Security Check: Illegal Characters
            if name.chars().any(|c| matches!(c, '<' | '>' | ':' | '"' | '|' | '?' | '*')) {
                return Err(anyhow::anyhow!("Illegal characters in path: {}", name));
            }

            // Security Check: Null Bytes or Control Characters
            if name.chars().any(|c| (c as u32) < 0x20) {
                return Err(anyhow::anyhow!("Control characters in path: {}", name));
            }

            // Security Check: Zip Bomb
            let compressed_size = file.compressed_size();
            let uncompressed_size = file.size();
            if uncompressed_size > 1024 * 1024 * 1024 { // 1GB limit
                return Err(anyhow::anyhow!("File too large in archive (potential zip bomb): {}", name));
            }
            if compressed_size > 0 && (uncompressed_size / compressed_size) > 100 {
                return Err(anyhow::anyhow!("High compression ratio (potential zip bomb): {}", name));
            }
        }

        Ok(())
    }
}
