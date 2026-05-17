#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <string>
#include "systemc"
#include "tlm.h"
#include "tlm_utils/multi_passthrough_target_socket.h"

namespace rv32 {

class Memory : public sc_core::sc_module {
public:
    static constexpr uint32_t SIZE = 8 * 1024 * 1024;  // 8MB

    tlm_utils::multi_passthrough_target_socket<Memory> socket;

    Memory(sc_core::sc_module_name name);

    void loadHex(const std::string& filename);
    uint32_t getStartPC() const { return start_pc; }

private:
    void b_transport(int id, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    uint32_t toOffset(uint64_t addr) const {
        int64_t offset = (int64_t)(addr - base_addr);
        if (offset >= 0 && offset < (int64_t)SIZE) return (uint32_t)offset;
        if (addr < SIZE) return (uint32_t)addr;
        return SIZE;
    }

    uint8_t mem[SIZE];
    uint32_t start_pc;
    uint64_t base_addr;
};

} // namespace rv32

#endif
