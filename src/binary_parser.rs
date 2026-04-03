use object::Object;
use std::collections::HashSet;
use std::fs;
use std::path::Path;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum BinaryFormat {
    Unknown,
    Elf,
    Pe,
    Macho,
}

#[derive(Debug, Clone, PartialEq)]
pub struct ArchInfo {
    pub bitness: i32,
    pub architecture: String,
}

#[derive(Debug, Clone, PartialEq)]
pub struct BinaryInfo {
    pub format: BinaryFormat,
    pub is_shared_library: bool,
    pub architectures: Vec<ArchInfo>,
    pub exports: HashSet<String>,
}

impl Default for BinaryInfo {
    fn default() -> Self {
        BinaryInfo {
            format: BinaryFormat::Unknown,
            is_shared_library: false,
            architectures: Vec::new(),
            exports: HashSet::new(),
        }
    }
}

pub struct BinaryParser;

impl BinaryParser {
    pub fn parse<P: AsRef<Path>>(path: P) -> anyhow::Result<BinaryInfo> {
        let file_data = fs::read(path)?;
        let mut info = BinaryInfo::default();

        match object::File::parse(&*file_data) {
            Ok(file) => {
                info.format = match file.format() {
                    object::BinaryFormat::Elf => BinaryFormat::Elf,
                    object::BinaryFormat::Pe => BinaryFormat::Pe,
                    object::BinaryFormat::MachO => BinaryFormat::Macho,
                    _ => BinaryFormat::Unknown,
                };

                // In object 0.32, kind check might be different or we use format specific checks
                info.is_shared_library = true; // Placeholder for now

                let arch = match file.architecture() {
                    object::Architecture::I386 => "x86",
                    object::Architecture::X86_64 => "x86_64",
                    object::Architecture::Aarch64 => "aarch64",
                    object::Architecture::Riscv64 => "riscv64",
                    object::Architecture::PowerPc => "ppc32",
                    object::Architecture::PowerPc64 => "ppc64",
                    _ => "unknown",
                };

                info.architectures.push(ArchInfo {
                    bitness: if file.is_64() { 64 } else { 32 },
                    architecture: arch.to_string(),
                });

                for symbol in file.exports().map_err(|e| anyhow::anyhow!(e))? {
                    if let Ok(name) = std::str::from_utf8(symbol.name()) {
                        info.exports.insert(name.to_string());
                    }
                }
            }
            Err(_) => {
                // Check for Mach-O Fat binaries which object::File might not handle in one go
                if let Ok(_fat) = object::macho::FatHeader::parse(&*file_data) {
                    info.format = BinaryFormat::Macho;
                    info.is_shared_library = true; // Typically used for libraries in FMI
                }
            }
        }

        Ok(info)
    }
}
