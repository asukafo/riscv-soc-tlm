#ifndef __MEMORY_INTERFACE_H__
#define __MEMORY_INTERFACE_H__

#include <cstdint>

#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"

namespace riscv_soc_tlm
{

class MemoryInterface
{
public:
    tlm_utils::simple_initiator_socket<MemoryInterface> socket;

    MemoryInterface();

    uint32_t readDataMem(uint64_t addr, int size);
    void writeDataMem(uint64_t addr, uint32_t data, int size);
};

}  // namespace riscv_soc_tlm

#endif
