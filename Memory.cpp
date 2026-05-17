#include "Memory.h"
#include <fstream>
#include <iostream>
#include <cstring>

namespace rv32 {

Memory::Memory(sc_core::sc_module_name name)
    : sc_core::sc_module(name), socket("socket"), start_pc(0x80000000), base_addr(0x80000000)
{
    socket.register_b_transport(this, &Memory::b_transport);
    std::memset(mem, 0, SIZE);
}

void Memory::b_transport(int /*id*/, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint64_t addr = trans.get_address();
    uint32_t offset = toOffset(addr);
    unsigned char* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();
    delay = sc_core::SC_ZERO_TIME;

    if (offset + len > SIZE) {
        std::cerr << "Memory access out of bounds: addr=0x" << std::hex << addr
                  << " offset=0x" << offset << " len=" << std::dec << len << std::endl;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        uint32_t val = 0;
        for (uint32_t i = 0; i < len; i++) {
            val |= static_cast<uint32_t>(mem[offset + i]) << (8 * i);
        }
        std::memcpy(ptr, &val, len);
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        uint32_t val = 0;
        std::memcpy(&val, ptr, len);
        for (uint32_t i = 0; i < len; i++) {
            mem[offset + i] = static_cast<uint8_t>((val >> (8 * i)) & 0xFF);
        }
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

static uint8_t hexChar(uint8_t c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

void Memory::loadHex(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open HEX file: " << filename << std::endl;
        return;
    }

    std::string line;
    uint32_t ext_addr = 0;
    uint32_t first_addr = 0;
    bool first_data = true;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] != ':') continue;

        size_t len = hexChar(line[1]) * 16 + hexChar(line[2]);
        uint32_t addr = (hexChar(line[3]) << 12) | (hexChar(line[4]) << 8) |
                        (hexChar(line[5]) << 4)  | hexChar(line[6]);
        uint8_t type = hexChar(line[7]) * 16 + hexChar(line[8]);

        if (type == 0x00) {
            uint32_t full_addr = addr + ext_addr;
            if (first_data) {
                base_addr = full_addr;
                first_addr = full_addr;
                first_data = false;
            }
            uint32_t offset = full_addr - base_addr;
            for (size_t i = 0; i < len && (offset + i) < SIZE; i++) {
                size_t pos = 9 + i * 2;
                mem[offset + i] = hexChar(line[pos]) * 16 + hexChar(line[pos + 1]);
            }
        } else if (type == 0x02) {
            ext_addr = 0;
            for (size_t i = 0; i < len; i++) {
                size_t pos = 9 + i * 2;
                ext_addr = (ext_addr << 8) | (hexChar(line[pos]) * 16 + hexChar(line[pos + 1]));
            }
            ext_addr <<= 4;
        } else if (type == 0x04) {
            ext_addr = 0;
            for (size_t i = 0; i < len; i++) {
                size_t pos = 9 + i * 2;
                ext_addr = (ext_addr << 8) | (hexChar(line[pos]) * 16 + hexChar(line[pos + 1]));
            }
            ext_addr <<= 16;
        } else if (type == 0x01) {
            break;
        }
    }

    if (!first_data) {
        start_pc = first_addr;
    }

    std::cout << "HEX loaded: " << filename << ", base=0x" << std::hex << base_addr
              << ", entry PC=0x" << start_pc << std::dec << std::endl;
}

} // namespace rv32
