#include "binary_checker.h"
#include "binary_parser.h"
#include "certificate.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

TEST_CASE("Binary Parser ELF", "[binary][elf]")
{
    // Create a dummy C file
    std::ofstream f("dummy.c");
    f << "void exported_func() {}\n";
    f.close();

    // Compile to shared library
    int res = std::system("gcc -shared -fPIC -o libdummy.so dummy.c");
    if (res != 0)
    {
        // Skip test if gcc is not available or fails
        return;
    }

    auto exports = BinaryParser::getExports("libdummy.so");
    CHECK(exports.contains("exported_func"));

    std::filesystem::remove("dummy.c");
    std::filesystem::remove("libdummy.so");
}

TEST_CASE("Binary Parser PE", "[binary][pe]")
{
    // Minimal PE file with an export named "fmi2Instantiate"
    std::vector<uint8_t> pe_data(2048, 0);

    auto write32 = [&](uint32_t off, uint32_t val) { std::memcpy(&pe_data[off], &val, 4); };
    auto write16 = [&](uint32_t off, uint16_t val) { std::memcpy(&pe_data[off], &val, 2); };

    // DOS Header
    pe_data[0] = 'M';
    pe_data[1] = 'Z';
    uint32_t pe_off = 0x80;
    write32(0x3C, pe_off);

    // PE Signature
    write32(pe_off, 0x00004550);

    // File Header
    write16(pe_off + 6, 1);    // 1 section
    write16(pe_off + 20, 240); // SizeOfOptionalHeader

    // Optional Header (PE32+)
    uint32_t opt_off = pe_off + 24;
    write16(opt_off, 0x020B);       // Magic 0x20B
    write32(opt_off + 108, 1);      // NumberOfRvaAndSizes
    write32(opt_off + 112, 0x1000); // Export Table RVA 0x1000
    write32(opt_off + 116, 0x100);  // Export Table Size 0x100

    // Section Header
    uint32_t sec_off = opt_off + 240;
    std::memcpy(&pe_data[sec_off], ".text", 5);
    write32(sec_off + 8, 0x1000);  // VirtualSize
    write32(sec_off + 12, 0x1000); // VirtualAddress
    write32(sec_off + 16, 0x400);  // SizeOfRawData
    write32(sec_off + 20, 0x400);  // PointerToRawData

    // Export Directory (at RVA 0x1000 -> File Offset 0x400)
    uint32_t exp_off = 0x400;
    write32(exp_off + 24, 1);      // NumberOfNames
    write32(exp_off + 32, 0x1030); // AddressOfNames (RVA)

    // AddressOfNames entry (at RVA 0x1030 -> File Offset 0x430)
    write32(0x430, 0x1040); // Name RVA

    // Name string (at RVA 0x1040 -> File Offset 0x440)
    std::string name = "fmi2Instantiate";
    std::memcpy(&pe_data[0x440], name.c_str(), name.size() + 1);

    std::ofstream f("test.dll", std::ios::binary);
    f.write((char*)pe_data.data(), pe_data.size());
    f.close();

    auto exports = BinaryParser::getExports("test.dll");
    CHECK(exports.contains("fmi2Instantiate"));

    std::filesystem::remove("test.dll");
}

TEST_CASE("Binary Parser Mach-O", "[binary][macho]")
{
    // Minimal Mach-O 64-bit with LC_SYMTAB
    std::vector<uint8_t> macho_data(1024, 0);

    auto write32 = [&](uint32_t off, uint32_t val) { std::memcpy(&macho_data[off], &val, 4); };

    // Mach Header
    write32(0, 0xFEEDFACF); // Magic
    write32(16, 1);         // ncmds
    write32(20, 24);        // sizeofcmds

    // LC_SYMTAB
    uint32_t lc_off = 32;
    write32(lc_off, 0x2);        // LC_SYMTAB
    write32(lc_off + 4, 24);     // cmdsize
    write32(lc_off + 8, 0x100);  // symoff
    write32(lc_off + 12, 1);     // nsyms
    write32(lc_off + 16, 0x200); // stroff
    write32(lc_off + 20, 0x100); // strsize

    // Symbol (nlist_64) at 0x100
    uint32_t sym_off = 0x100;
    write32(sym_off, 1);            // n_strx (offset 1 in strtab)
    macho_data[sym_off + 4] = 0x0f; // n_type (N_EXT | N_SECT)
    macho_data[sym_off + 5] = 1;    // n_sect

    // String Table at 0x200
    // Index 0 is empty string
    std::string name = "_fmi2Instantiate";
    std::memcpy(&macho_data[0x201], name.c_str(), name.size() + 1);

    std::ofstream f("test.dylib", std::ios::binary);
    f.write((char*)macho_data.data(), macho_data.size());
    f.close();

    auto exports = BinaryParser::getExports("test.dylib");
    CHECK(exports.contains("fmi2Instantiate"));

    std::filesystem::remove("test.dylib");
}

TEST_CASE("Binary Checker Validation", "[binary][checker]")
{
    // Create a dummy FMU directory
    fs::create_directories("test_fmu/binaries/linux64");

    std::ofstream md("test_fmu/modelDescription.xml");
    md << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    md << "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"Test\" guid=\"{123}\">\n";
    md << "  <CoSimulation modelIdentifier=\"Test\"/>\n";
    md << "  <ModelVariables/>\n";
    md << "  <ModelStructure/>\n";
    md << "</fmiModelDescription>\n";
    md.close();

    // Create a dummy binary with NO exports
    std::ofstream bin("test_fmu/binaries/linux64/Test.so");
    bin << "not a real elf";
    bin.close();

    BinaryChecker checker;
    Certificate cert;
    checker.validate("test_fmu", cert);

    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "Mandatory function 'fmi2GetVersion' is not exported."));

    fs::remove_all("test_fmu");
}
