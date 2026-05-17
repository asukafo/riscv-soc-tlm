#ifndef __DMA_H__
#define __DMA_H__

#include <cstdint>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"

namespace riscv_soc_tlm
{

class DMA : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<DMA> initiator_socket;

    DMA(sc_core::sc_module_name name);

    void transfer(uint64_t src, uint64_t dst, uint32_t size);

private:
    uint32_t readWord(uint64_t addr);
    void writeWord(uint64_t addr, uint32_t data);
};

}  // namespace riscv_soc_tlm

#endif
