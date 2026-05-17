#ifndef MEMORY_INTERFACE_H
#define MEMORY_INTERFACE_H

#include <cstdint>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"

namespace rv32 {

class MemoryInterface {
public:
    tlm_utils::simple_initiator_socket<MemoryInterface> socket;

    MemoryInterface();

    uint32_t readDataMem(uint64_t addr, int size);
    void writeDataMem(uint64_t addr, uint32_t data, int size);
    uint32_t fetchInstruction(uint32_t addr);
};

} // namespace rv32

#endif
