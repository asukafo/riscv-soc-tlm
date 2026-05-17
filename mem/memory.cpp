#include "mem/memory.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace riscv_soc_tlm
{

static uint32_t read32LE(const unsigned char* buf)
{
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

Memory::Memory(sc_core::sc_module_name name)
    : sc_core::sc_module(name), socket("socket"), base_addr(0x80000000)
{
    socket.register_b_transport(this, &Memory::b_transport);
    std::memset(mem, 0, SIZE);
}

void Memory::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();
    uint32_t offset = toOffset(addr);
    unsigned char* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();
    delay = sc_core::SC_ZERO_TIME;

    if (offset + len > SIZE)
    {
        std::cerr << "Memory access out of bounds: addr=0x" << std::hex << addr << " offset=0x"
                  << offset << " len=" << std::dec << len << std::endl;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND)
    {
        for (uint32_t i = 0; i < len; i++)
        {
            ptr[i] = mem[offset + i];
        }
    }
    else if (trans.get_command() == tlm::TLM_WRITE_COMMAND)
    {
        for (uint32_t i = 0; i < len; i++)
        {
            mem[offset + i] = ptr[i];
        }
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

// ─── Intel HEX loader ──────────────────────────────────────────────

static uint8_t hexChar(uint8_t c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

uint32_t Memory::loadHex(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Cannot open HEX file: " << filename << std::endl;
        return 0x80000000;
    }

    std::string line;
    uint32_t ext_addr = 0;
    uint32_t first_addr = 0;
    uint32_t start_pc = 0x80000000;
    bool first_data = true;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] != ':') continue;

        size_t len = hexChar(line[1]) * 16 + hexChar(line[2]);
        uint32_t addr = (hexChar(line[3]) << 12) | (hexChar(line[4]) << 8) |
                        (hexChar(line[5]) << 4) | hexChar(line[6]);
        uint8_t type = hexChar(line[7]) * 16 + hexChar(line[8]);

        if (type == 0x00)
        {
            uint32_t full_addr = addr + ext_addr;
            if (first_data)
            {
                base_addr = full_addr;
                first_addr = full_addr;
                first_data = false;
            }
            uint32_t offset = full_addr - base_addr;
            for (size_t i = 0; i < len && (offset + i) < SIZE; i++)
            {
                size_t pos = 9 + i * 2;
                mem[offset + i] = hexChar(line[pos]) * 16 + hexChar(line[pos + 1]);
            }
        }
        else if (type == 0x04)
        {
            ext_addr = 0;
            for (size_t i = 0; i < len; i++)
            {
                size_t pos = 9 + i * 2;
                ext_addr = (ext_addr << 8) | (hexChar(line[pos]) * 16 + hexChar(line[pos + 1]));
            }
            ext_addr <<= 16;
        }
        else if (type == 0x01)
        {
            break;
        }
    }

    if (!first_data) start_pc = first_addr;

    std::cout << "HEX loaded: " << filename << ", base=0x" << std::hex << base_addr
              << ", entry PC=0x" << start_pc << std::dec << std::endl;

    return start_pc;
}

// ─── ELF loader ─────────────────────────────────────────────────────

uint32_t Memory::loadELF(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Cannot open ELF file: " << filename << std::endl;
        return 0x80000000;
    }

    // Read ELF header (52 bytes for 32-bit)
    unsigned char ehdr[52];
    file.read(reinterpret_cast<char*>(ehdr), sizeof(ehdr));

    // Verify ELF magic
    if (ehdr[0] != 0x7F || ehdr[1] != 'E' || ehdr[2] != 'L' || ehdr[3] != 'F')
    {
        std::cerr << "Not a valid ELF file: " << filename << std::endl;
        return 0x80000000;
    }

    if (ehdr[4] != 1) // ELFCLASS32
    {
        std::cerr << "ELF is not 32-bit: " << filename << std::endl;
        return 0x80000000;
    }

    // Entry point
    uint32_t entry = read32LE(ehdr + 24);

    // Program header offset, entry size, count
    uint32_t phoff = read32LE(ehdr + 28);
    uint16_t phentsize = ehdr[42] | (ehdr[43] << 8);
    uint16_t phnum = ehdr[44] | (ehdr[45] << 8);

    // Read program headers
    for (uint16_t i = 0; i < phnum; i++)
    {
        uint32_t phdr_off = phoff + i * phentsize;
        file.seekg(phdr_off);
        unsigned char phdr[32];
        file.read(reinterpret_cast<char*>(phdr), sizeof(phdr));

        uint32_t p_type = read32LE(phdr);
        if (p_type != 1) continue; // PT_LOAD only

        uint32_t p_offset = read32LE(phdr + 4);
        uint32_t p_vaddr = read32LE(phdr + 8);
        uint32_t p_filesz = read32LE(phdr + 16);
        uint32_t p_memsz = read32LE(phdr + 20);

        uint32_t offset = toOffset(p_vaddr);
        if (offset + p_memsz > SIZE)
        {
            std::cerr << "ELF segment exceeds memory: vaddr=0x" << std::hex << p_vaddr
                      << " memsz=0x" << p_memsz << std::dec << std::endl;
            return entry;
        }

        // Copy segment data
        if (p_filesz > 0)
        {
            file.seekg(p_offset);
            file.read(reinterpret_cast<char*>(mem + offset), p_filesz);
        }
        // Zero-fill .bss
        if (p_memsz > p_filesz)
        {
            std::memset(mem + offset + p_filesz, 0, p_memsz - p_filesz);
        }

        if (i == 0)
        {
            base_addr = p_vaddr;
        }
    }

    std::cout << "ELF loaded: " << filename << ", base=0x" << std::hex << base_addr
              << ", entry PC=0x" << entry << std::dec << std::endl;

    return entry;
}

}  // namespace riscv_soc_tlm
