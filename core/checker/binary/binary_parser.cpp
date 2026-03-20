#include "binary_parser.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <set>
#include <string>
#include <vector>

// --- ELF Parsing Structures ---
#pragma pack(push, 1)
struct Elf64_Ehdr
{
    std::array<unsigned char, 16> e_ident;
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf32_Ehdr
{
    std::array<unsigned char, 16> e_ident;
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Phdr
{
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct Elf32_Phdr
{
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

struct Elf64_Dyn
{
    int64_t d_tag;
    union
    {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
};

struct Elf32_Dyn
{
    int32_t d_tag;
    union
    {
        uint32_t d_val;
        uint32_t d_ptr;
    } d_un;
};

struct Elf64_Sym
{
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};

struct Elf32_Sym
{
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
};

struct Elf64_Shdr
{
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

struct Elf32_Shdr
{
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};
#pragma pack(pop)

enum ElfConstants : uint32_t
{
    PT_LOAD = 1,
    PT_DYNAMIC = 2,
    DT_NULL = 0,
    DT_SYMTAB = 6,
    DT_STRTAB = 5,
    DT_HASH = 4,
    DT_GNU_HASH = 0x6ffffef5,
    SHT_DYNSYM = 11,
    SHN_UNDEF = 0,
    STB_LOCAL = 0,
    EI_NIDENT = 16,
    EI_CLASS = 4,
    ELFCLASS32 = 1,
    ELFCLASS64 = 2,
    ELFMAG0 = 0x7f
};

static inline uint8_t ELF64_ST_BIND(uint8_t i)
{
    return i >> 4;
}
static inline uint8_t ELF32_ST_BIND(uint8_t i)
{
    return i >> 4;
}

template <typename T>
static bool readFromFile(std::ifstream& f, std::streamoff offset, T& dest)
{
    f.seekg(offset);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return !!f.read(reinterpret_cast<char*>(&dest), sizeof(T));
}

static std::set<std::string> parseElf64(std::ifstream& f)
{
    std::set<std::string> exports;
    Elf64_Ehdr ehdr{};
    if (!readFromFile(f, 0, ehdr))
        return {};

    // Find PT_DYNAMIC
    Elf64_Phdr phdr{};
    bool found_dynamic = false;
    for (int i = 0; i < ehdr.e_phnum; ++i)
    {
        if (readFromFile(f, static_cast<std::streamoff>(ehdr.e_phoff + static_cast<uint64_t>(i) * ehdr.e_phentsize),
                         phdr))
        {
            if (phdr.p_type == PT_DYNAMIC)
            {
                found_dynamic = true;
                break;
            }
        }
    }

    if (!found_dynamic)
        return {};

    // Read Dynamic entries
    std::vector<Elf64_Dyn> dyns;
    Elf64_Dyn dyn{};
    f.seekg(static_cast<std::streamoff>(phdr.p_offset));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    while (f.read(reinterpret_cast<char*>(&dyn), sizeof(dyn)) && dyn.d_tag != DT_NULL)
        dyns.push_back(dyn);

    uint64_t symtab_off = 0;
    uint64_t strtab_off = 0;
    uint64_t hash_off = 0;
    uint64_t gnu_hash_off = 0;

    for (const auto& d : dyns)
    {
        switch (d.d_tag)
        {
        case DT_SYMTAB:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            symtab_off = d.d_un.d_ptr;
            break;
        case DT_STRTAB:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            strtab_off = d.d_un.d_ptr;
            break;
        case DT_HASH:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            hash_off = d.d_un.d_ptr;
            break;
        case DT_GNU_HASH:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            gnu_hash_off = d.d_un.d_ptr;
            break;
        default:
            break;
        }
    }

    // In a shared object, DT_SYMTAB etc. might contain virtual addresses.
    // We need to map these back to file offsets using program headers.
    auto va_to_off = [&](uint64_t va) -> uint64_t
    {
        f.seekg(static_cast<std::streamoff>(ehdr.e_phoff));
        for (int i = 0; i < ehdr.e_phnum; ++i)
        {
            Elf64_Phdr p{};
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            if (f.read(reinterpret_cast<char*>(&p), sizeof(p)))
            {
                if (p.p_type == PT_LOAD && va >= p.p_vaddr && va < p.p_vaddr + p.p_memsz)
                    return va - p.p_vaddr + p.p_offset;
            }
        }
        return va; // Fallback
    };

    symtab_off = va_to_off(symtab_off);
    strtab_off = va_to_off(strtab_off);
    hash_off = va_to_off(hash_off);
    gnu_hash_off = va_to_off(gnu_hash_off);

    uint32_t nsyms = 0;
    if (hash_off != 0)
    {
        // Read nchain from DT_HASH
        uint32_t nbucket = 0;
        if (readFromFile(f, static_cast<std::streamoff>(hash_off), nbucket))
            readFromFile(f, static_cast<std::streamoff>(hash_off + 4), nsyms);
    }
    else if (gnu_hash_off != 0)
    {
        // DT_GNU_HASH is harder, but we can find the number of symbols by looking at the buckets and chains.
        uint32_t nbuckets = 0, symoffset = 0, bloom_size = 0, bloom_shift = 0;
        readFromFile(f, static_cast<std::streamoff>(gnu_hash_off), nbuckets);
        readFromFile(f, static_cast<std::streamoff>(gnu_hash_off + 4), symoffset);
        readFromFile(f, static_cast<std::streamoff>(gnu_hash_off + 8), bloom_size);
        readFromFile(f, static_cast<std::streamoff>(gnu_hash_off + 12), bloom_shift);

        uint32_t max_idx = symoffset;
        std::vector<uint32_t> buckets(nbuckets);
        f.seekg(static_cast<std::streamoff>(gnu_hash_off + 16 + static_cast<uint64_t>(bloom_size) * 8));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        f.read(reinterpret_cast<char*>(buckets.data()), static_cast<std::streamsize>(nbuckets) * 4);

        for (const uint32_t b : buckets)
            if (b > max_idx)
                max_idx = b;

        if (max_idx > 0)
        {
            // Iterate through the chains until we find the last one (bit 0 of chain entry is 1)
            bool done = false;
            uint32_t curr_idx = max_idx;
            while (!done)
            {
                uint32_t chain_val = 0;
                if (readFromFile(f,
                                 static_cast<std::streamoff>(gnu_hash_off + 16 + static_cast<uint64_t>(bloom_size) * 8 +
                                                             static_cast<uint64_t>(nbuckets) * 4 +
                                                             static_cast<uint64_t>(curr_idx - symoffset) * 4),
                                 chain_val))
                {
                    if (chain_val & 1)
                        done = true;
                    curr_idx++;
                }
                else
                {
                    done = true;
                }
            }
            nsyms = curr_idx;
        }
    }

    if (nsyms == 0)
    {
        // Fallback: use section headers if available
        for (int i = 0; i < ehdr.e_shnum; ++i)
        {
            Elf64_Shdr shdr{};
            if (readFromFile(f, static_cast<std::streamoff>(ehdr.e_shoff + static_cast<uint64_t>(i) * ehdr.e_shentsize),
                             shdr))
            {
                if (shdr.sh_type == SHT_DYNSYM)
                {
                    nsyms = static_cast<uint32_t>(shdr.sh_size / shdr.sh_entsize);
                    symtab_off = shdr.sh_offset;
                    // Get strtab too
                    Elf64_Shdr str_shdr{};
                    if (readFromFile(f,
                                     static_cast<std::streamoff>(ehdr.e_shoff + static_cast<uint64_t>(shdr.sh_link) *
                                                                                    ehdr.e_shentsize),
                                     str_shdr))
                        strtab_off = str_shdr.sh_offset;
                    break;
                }
            }
        }
    }

    for (uint32_t i = 0; i < nsyms; ++i)
    {
        Elf64_Sym sym{};
        if (readFromFile(f, static_cast<std::streamoff>(symtab_off + static_cast<uint64_t>(i) * sizeof(Elf64_Sym)),
                         sym))
        {
            if (ELF64_ST_BIND(sym.st_info) != STB_LOCAL && sym.st_shndx != SHN_UNDEF)
            {
                f.seekg(static_cast<std::streamoff>(strtab_off + sym.st_name));
                std::string name;
                char c = 0;
                while (f.get(c) && c != '\0')
                    name += c;
                if (!name.empty())
                    exports.insert(name);
            }
        }
    }

    return exports;
}

// --- Mach-O Parsing Structures ---
#pragma pack(push, 1)
struct mach_header_64
{
    uint32_t magic;
    int32_t cputype;
    int32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
};

struct mach_header
{
    uint32_t magic;
    int32_t cputype;
    int32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
};

struct load_command
{
    uint32_t cmd;
    uint32_t cmdsize;
};

struct symtab_command
{
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;
};

struct dyld_info_command
{
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t rebase_off;
    uint32_t rebase_size;
    uint32_t bind_off;
    uint32_t bind_size;
    uint32_t weak_bind_off;
    uint32_t weak_bind_size;
    uint32_t lazy_bind_off;
    uint32_t lazy_bind_size;
    uint32_t export_off;
    uint32_t export_size;
};

struct linkedit_data_command
{
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t dataoff;
    uint32_t datasize;
};

struct nlist_64
{
    uint32_t n_strx;
    uint8_t n_type;
    uint8_t n_sect;
    uint16_t n_desc;
    uint64_t n_value;
};

struct nlist
{
    uint32_t n_strx;
    uint8_t n_type;
    uint8_t n_sect;
    uint16_t n_desc;
    uint32_t n_value;
};

struct fat_header
{
    uint32_t magic;
    uint32_t nfat_arch;
};

struct fat_arch
{
    int32_t cputype;
    int32_t cpusubtype;
    uint32_t offset;
    uint32_t size;
    uint32_t align;
};
#pragma pack(pop)

static uint64_t readUleb128(std::ifstream& f)
{
    uint64_t result = 0;
    int shift = 0;
    while (true)
    {
        uint8_t byte = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!f.read(reinterpret_cast<char*>(&byte), 1))
            break;
        result |= static_cast<uint64_t>(byte & 0x7f) << shift;
        if (!(byte & 0x80))
            break;
        shift += 7;
    }
    return result;
}

static void walkTrie(std::ifstream& f, uint32_t start_off, uint32_t curr_off, const std::string& prefix,
                     std::set<std::string>& exports)
{
    f.seekg(start_off + curr_off);
    uint64_t terminalSize = readUleb128(f);
    if (terminalSize != 0)
    {
        // This is an export.
        // FMI symbols usually start with underscore in Mach-O
        if (prefix.starts_with('_'))
            exports.insert(prefix.substr(1));
        else
            exports.insert(prefix);
    }

    // Skip terminal data
    f.seekg(static_cast<std::streamoff>(start_off + curr_off + 1 + terminalSize)); // Rough estimate of ULEB size as 1

    // Need to correctly find children
    f.seekg(start_off + curr_off);
    terminalSize = readUleb128(f);
    f.seekg(static_cast<std::streamoff>(start_off + curr_off + (terminalSize > 0 ? (1 + terminalSize) : 1)));

    // Actually we should save the position after terminalSize
    // Let's do it properly
    f.seekg(start_off + curr_off);
    uint8_t b = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    f.read(reinterpret_cast<char*>(&b), 1);
    uint64_t tSize = b;
    if (b & 0x80)
    {
        f.seekg(start_off + curr_off);
        tSize = readUleb128(f);
    }
    const uint32_t children_pos = static_cast<uint32_t>(f.tellg()) + static_cast<uint32_t>(tSize);

    f.seekg(children_pos);
    uint8_t childCount = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (!f.read(reinterpret_cast<char*>(&childCount), 1))
        return;

    for (uint8_t i = 0; i < childCount; ++i)
    {
        std::string edgeLabel;
        char c = 0;
        while (f.get(c) && c != '\0')
            edgeLabel += c;
        const uint64_t childOffset = readUleb128(f);
        const uint32_t next_pos = static_cast<uint32_t>(f.tellg());
        walkTrie(f, start_off, static_cast<uint32_t>(childOffset), prefix + edgeLabel, exports);
        f.seekg(next_pos);
    }
}

static constexpr uint32_t swap32(uint32_t x)
{
    return (x << 24) | ((x << 8) & 0xFF0000) | ((x >> 8) & 0xFF00) | (x >> 24);
}

static std::set<std::string> parseMachO(std::ifstream& f, uint32_t base_off)
{
    std::set<std::string> exports;
    uint32_t magic = 0;
    if (!readFromFile(f, base_off, magic))
        return {};

    const bool is_64 = (magic == 0xFEEDFACF || magic == 0xCFFAEDFE);
    const bool swap = (magic == 0xCEFAEDFE || magic == 0xCFFAEDFE);

    uint32_t ncmds = 0;
    uint32_t header_size = 0;

    if (is_64)
    {
        mach_header_64 h{};
        readFromFile(f, base_off, h);
        ncmds = swap ? swap32(h.ncmds) : h.ncmds;
        header_size = sizeof(mach_header_64);
    }
    else
    {
        mach_header h{};
        readFromFile(f, base_off, h);
        ncmds = swap ? swap32(h.ncmds) : h.ncmds;
        header_size = sizeof(mach_header);
    }

    uint32_t curr_off = base_off + header_size;
    uint32_t symoff = 0, nsyms = 0, stroff = 0;
    uint32_t export_off = 0;

    for (uint32_t i = 0; i < ncmds; ++i)
    {
        load_command lc{};
        readFromFile(f, curr_off, lc);
        const uint32_t cmd = swap ? swap32(lc.cmd) : lc.cmd;
        const uint32_t cmdsize = swap ? swap32(lc.cmdsize) : lc.cmdsize;

        if (cmd == 0x2 /* LC_SYMTAB */)
        {
            symtab_command sc{};
            readFromFile(f, curr_off, sc);
            symoff = swap ? swap32(sc.symoff) : sc.symoff;
            nsyms = swap ? swap32(sc.nsyms) : sc.nsyms;
            stroff = swap ? swap32(sc.stroff) : sc.stroff;
        }
        else if (cmd == 0x80000022 /* LC_DYLD_INFO_ONLY */ || cmd == 0x22 /* LC_DYLD_INFO */)
        {
            dyld_info_command dc{};
            readFromFile(f, curr_off, dc);
            export_off = swap ? swap32(dc.export_off) : dc.export_off;
        }
        else if (cmd == 0x48 /* LC_DYLD_EXPORTS_TRIE */)
        {
            linkedit_data_command dc{};
            readFromFile(f, curr_off, dc);
            export_off = swap ? swap32(dc.dataoff) : dc.dataoff;
        }
        curr_off += cmdsize;
    }

    if (export_off != 0)
    {
        walkTrie(f, base_off + export_off, 0, "", exports);
    }
    else if (symoff != 0)
    {
        for (uint32_t i = 0; i < nsyms; ++i)
        {
            uint32_t strx = 0;
            uint8_t type = 0;
            if (is_64)
            {
                nlist_64 nl{};
                readFromFile(f, static_cast<std::streamoff>(base_off + symoff + i * sizeof(nlist_64)), nl);
                strx = swap ? swap32(nl.n_strx) : nl.n_strx;
                type = nl.n_type;
            }
            else
            {
                nlist nl{};
                readFromFile(f, static_cast<std::streamoff>(base_off + symoff + i * sizeof(nlist)), nl);
                strx = swap ? swap32(nl.n_strx) : nl.n_strx;
                type = nl.n_type;
            }

            if ((type & 0x01) /* N_EXT */ && (type & 0x0e) != 0 /* not undefined */)
            {
                f.seekg(base_off + stroff + strx);
                std::string name;
                char c = 0;
                while (f.get(c) && c != '\0')
                    name += c;
                if (!name.empty())
                {
                    if (name.starts_with('_'))
                        exports.insert(name.substr(1));
                    else
                        exports.insert(name);
                }
            }
        }
    }

    return exports;
}

static std::set<std::string> parseElf32(std::ifstream& f)
{
    std::set<std::string> exports;
    Elf32_Ehdr ehdr{};
    if (!readFromFile(f, 0, ehdr))
        return {};

    // Find PT_DYNAMIC
    Elf32_Phdr phdr{};
    bool found_dynamic = false;
    for (int i = 0; i < ehdr.e_phnum; ++i)
    {
        if (readFromFile(f, static_cast<std::streamoff>(ehdr.e_phoff + static_cast<uint32_t>(i) * ehdr.e_phentsize),
                         phdr))
        {
            if (phdr.p_type == PT_DYNAMIC)
            {
                found_dynamic = true;
                break;
            }
        }
    }

    if (!found_dynamic)
        return {};

    // Read Dynamic entries
    std::vector<Elf32_Dyn> dyns;
    Elf32_Dyn dyn{};
    f.seekg(static_cast<std::streamoff>(phdr.p_offset));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    while (f.read(reinterpret_cast<char*>(&dyn), sizeof(dyn)) && dyn.d_tag != DT_NULL)
        dyns.push_back(dyn);

    uint32_t symtab_off = 0;
    uint32_t strtab_off = 0;
    uint32_t hash_off = 0;
    uint32_t gnu_hash_off = 0;

    for (const auto& d : dyns)
    {
        switch (d.d_tag)
        {
        case DT_SYMTAB:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            symtab_off = d.d_un.d_ptr;
            break;
        case DT_STRTAB:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            strtab_off = d.d_un.d_ptr;
            break;
        case DT_HASH:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            hash_off = d.d_un.d_ptr;
            break;
        case DT_GNU_HASH:
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            gnu_hash_off = d.d_un.d_ptr;
            break;
        default:
            break;
        }
    }

    auto va_to_off = [&](uint32_t va) -> uint32_t
    {
        f.seekg(static_cast<std::streamoff>(ehdr.e_phoff));
        for (int i = 0; i < ehdr.e_phnum; ++i)
        {
            Elf32_Phdr p{};
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            if (f.read(reinterpret_cast<char*>(&p), sizeof(p)))
            {
                if (p.p_type == PT_LOAD && va >= p.p_vaddr && va < p.p_vaddr + p.p_memsz)
                    return va - p.p_vaddr + p.p_offset;
            }
        }
        return va;
    };

    symtab_off = va_to_off(symtab_off);
    strtab_off = va_to_off(strtab_off);
    hash_off = va_to_off(hash_off);
    gnu_hash_off = va_to_off(gnu_hash_off);

    uint32_t nsyms = 0;
    if (hash_off != 0)
    {
        uint32_t nbucket = 0;
        if (readFromFile(f, static_cast<std::streamoff>(hash_off), nbucket))
            readFromFile(f, static_cast<std::streamoff>(hash_off + 4), nsyms);
    }
    else if (gnu_hash_off != 0)
    {
        uint32_t nbuckets = 0, symoffset = 0, bloom_size = 0, bloom_shift = 0;
        readFromFile(f, static_cast<std::streamoff>(gnu_hash_off), nbuckets);
        readFromFile(f, static_cast<std::streamoff>(gnu_hash_off + 4), symoffset);
        readFromFile(f, static_cast<std::streamoff>(gnu_hash_off + 8), bloom_size);
        readFromFile(f, static_cast<std::streamoff>(gnu_hash_off + 12), bloom_shift);

        uint32_t max_idx = symoffset;
        std::vector<uint32_t> buckets(nbuckets);
        f.seekg(static_cast<std::streamoff>(gnu_hash_off + 16 + static_cast<uint32_t>(bloom_size) * 4));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        f.read(reinterpret_cast<char*>(buckets.data()), static_cast<std::streamsize>(nbuckets) * 4);

        for (const uint32_t b : buckets)
            if (b > max_idx)
                max_idx = b;

        if (max_idx > 0)
        {
            bool done = false;
            uint32_t curr_idx = max_idx;
            while (!done)
            {
                uint32_t chain_val = 0;
                if (readFromFile(f,
                                 static_cast<std::streamoff>(gnu_hash_off + 16 + static_cast<uint32_t>(bloom_size) * 4 +
                                                             static_cast<uint32_t>(nbuckets) * 4 +
                                                             static_cast<uint32_t>(curr_idx - symoffset) * 4),
                                 chain_val))
                {
                    if (chain_val & 1)
                        done = true;
                    curr_idx++;
                }
                else
                {
                    done = true;
                }
            }
            nsyms = curr_idx;
        }
    }

    if (nsyms == 0)
    {
        // Fallback: use section headers if available
        for (int i = 0; i < ehdr.e_shnum; ++i)
        {
            Elf32_Shdr shdr{};
            if (readFromFile(f, static_cast<std::streamoff>(ehdr.e_shoff + static_cast<uint32_t>(i) * ehdr.e_shentsize),
                             shdr))
            {
                if (shdr.sh_type == SHT_DYNSYM)
                {
                    nsyms = static_cast<uint32_t>(shdr.sh_size / shdr.sh_entsize);
                    symtab_off = shdr.sh_offset;
                    // Get strtab too
                    Elf32_Shdr str_shdr{};
                    if (readFromFile(f,
                                     static_cast<std::streamoff>(ehdr.e_shoff + static_cast<uint32_t>(shdr.sh_link) *
                                                                                    ehdr.e_shentsize),
                                     str_shdr))
                        strtab_off = str_shdr.sh_offset;
                    break;
                }
            }
        }
    }

    for (uint32_t i = 0; i < nsyms; ++i)
    {
        Elf32_Sym sym{};
        if (readFromFile(f, static_cast<std::streamoff>(symtab_off + static_cast<uint32_t>(i) * sizeof(Elf32_Sym)),
                         sym))
        {
            if (ELF32_ST_BIND(sym.st_info) != STB_LOCAL && sym.st_shndx != SHN_UNDEF)
            {
                f.seekg(static_cast<std::streamoff>(strtab_off + sym.st_name));
                std::string name;
                char c = 0;
                while (f.get(c) && c != '\0')
                    name += c;
                if (!name.empty())
                    exports.insert(name);
            }
        }
    }

    return exports;
}

// --- PE Parsing Structures ---
#pragma pack(push, 1)
struct IMAGE_DOS_HEADER
{
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    std::array<uint16_t, 4> e_res;
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    std::array<uint16_t, 10> e_res2;
    uint32_t e_lfanew;
};

struct IMAGE_FILE_HEADER
{
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct IMAGE_DATA_DIRECTORY
{
    uint32_t VirtualAddress;
    uint32_t Size;
};

struct IMAGE_OPTIONAL_HEADER32
{
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    std::array<IMAGE_DATA_DIRECTORY, 16> DataDirectory;
};

struct IMAGE_OPTIONAL_HEADER64
{
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t ImageBaseHigh; // Wait, actually it's uint64_t ImageBase; the fields are different
};

// Redefining IMAGE_OPTIONAL_HEADER64 properly
struct IMAGE_OPTIONAL_HEADER64_REAL
{
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    std::array<IMAGE_DATA_DIRECTORY, 16> DataDirectory;
};

struct IMAGE_SECTION_HEADER
{
    std::array<uint8_t, 8> Name;
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

struct IMAGE_EXPORT_DIRECTORY
{
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;
};
#pragma pack(pop)

static std::set<std::string> parsePe(std::ifstream& f)
{
    std::set<std::string> exports;
    IMAGE_DOS_HEADER dos{};
    if (!readFromFile(f, 0, dos) || dos.e_magic != 0x5A4D)
        return {};

    uint32_t pe_sig = 0;
    if (!readFromFile(f, dos.e_lfanew, pe_sig) || pe_sig != 0x00004550)
        return {};

    IMAGE_FILE_HEADER file_hdr{};
    if (!readFromFile(f, dos.e_lfanew + 4, file_hdr))
        return {};

    const uint32_t optional_hdr_off = dos.e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER);
    uint16_t magic = 0;
    if (!readFromFile(f, optional_hdr_off, magic))
        return {};

    IMAGE_DATA_DIRECTORY export_dir_info = {0, 0};
    if (magic == 0x10B) // PE32
    {
        IMAGE_OPTIONAL_HEADER32 opt{};
        if (readFromFile(f, optional_hdr_off, opt))
        {
            if (opt.NumberOfRvaAndSizes > 0)
                export_dir_info = opt.DataDirectory[0];
        }
    }
    else if (magic == 0x20B) // PE32+
    {
        IMAGE_OPTIONAL_HEADER64_REAL opt{};
        if (readFromFile(f, optional_hdr_off, opt))
        {
            if (opt.NumberOfRvaAndSizes > 0)
                export_dir_info = opt.DataDirectory[0];
        }
    }

    if (export_dir_info.VirtualAddress == 0)
        return {};

    // Find section containing export directory
    std::vector<IMAGE_SECTION_HEADER> sections(file_hdr.NumberOfSections);
    const uint32_t section_hdr_off = optional_hdr_off + file_hdr.SizeOfOptionalHeader;
    f.seekg(section_hdr_off);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    f.read(reinterpret_cast<char*>(sections.data()),
           static_cast<std::streamsize>(sections.size() * sizeof(IMAGE_SECTION_HEADER)));

    auto rva_to_off = [&](uint32_t rva) -> uint32_t
    {
        for (const auto& sec : sections)
            if (rva >= sec.VirtualAddress && rva < sec.VirtualAddress + sec.VirtualSize)
                return rva - sec.VirtualAddress + sec.PointerToRawData;
        return 0;
    };

    const uint32_t export_dir_off = rva_to_off(export_dir_info.VirtualAddress);
    if (export_dir_off == 0)
        return {};

    IMAGE_EXPORT_DIRECTORY export_dir{};
    if (!readFromFile(f, export_dir_off, export_dir))
        return {};

    const uint32_t names_off = rva_to_off(export_dir.AddressOfNames);
    if (names_off == 0)
        return {};

    for (uint32_t i = 0; i < export_dir.NumberOfNames; ++i)
    {
        uint32_t name_rva = 0;
        if (readFromFile(f, names_off + i * 4, name_rva))
        {
            const uint32_t name_off = rva_to_off(name_rva);
            if (name_off != 0)
            {
                f.seekg(name_off);
                std::string name;
                char c = 0;
                while (f.get(c) && c != '\0')
                    name += c;
                if (!name.empty())
                    exports.insert(name);
            }
        }
    }

    return exports;
}

std::set<std::string> BinaryParser::getExports(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return {};

    uint32_t magic = 0;
    if (!readFromFile(f, 0, magic))
        return {};

    if (magic == 0x464c457f /* ELF in little-endian */ || magic == 0x7f454c46 /* ELF in big-endian */)
    {
        f.seekg(0);
        std::array<unsigned char, EI_NIDENT> ident{};
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        f.read(reinterpret_cast<char*>(ident.data()), static_cast<std::streamsize>(ident.size()));
        if (ident[EI_CLASS] == ELFCLASS64)
            return parseElf64(f);
        else if (ident[EI_CLASS] == ELFCLASS32)
            return parseElf32(f);
    }
    else if ((magic & 0xFFFF) == 0x5A4D /* MZ */)
    {
        return parsePe(f);
    }
    else if (magic == 0xFEEDFACE || magic == 0xFEEDFACF || magic == 0xCEFAEDFE || magic == 0xCFFAEDFE)
    {
        return parseMachO(f, 0);
    }
    else if (magic == 0xCAFEBABE || magic == 0xBEBAFECA) // Fat Binary
    {
        fat_header fh{};
        readFromFile(f, 0, fh);
        const uint32_t nfat = (magic == 0xBEBAFECA) ? swap32(fh.nfat_arch) : fh.nfat_arch;

        for (uint32_t i = 0; i < nfat; ++i)
        {
            fat_arch fa{};
            readFromFile(f, static_cast<std::streamoff>(sizeof(fat_header) + i * sizeof(fat_arch)), fa);
            const uint32_t offset = (magic == 0xBEBAFECA) ? swap32(fa.offset) : fa.offset;
            // For FMUs, we just take the first architecture that we can parse
            auto res = parseMachO(f, offset);
            if (!res.empty())
                return res;
        }
    }

    return {};
}
