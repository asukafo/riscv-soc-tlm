#ifndef __DMA_H__
#define __DMA_H__

#include <cstdint>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

namespace riscv_soc_tlm
{

class DMA : public sc_core::sc_module
{
public:
    // CPU writes MMIO registers via this socket
    tlm_utils::simple_target_socket<DMA> target_socket;
    // DMA reads/writes memory via this socket
    tlm_utils::simple_initiator_socket<DMA> initiator_socket;

    DMA(sc_core::sc_module_name name);

private:
    // MMIO registers (offsets from base 0x10000000)
    uint64_t m_src_addr;   // +0x0
    uint64_t m_dst_addr;   // +0x4 (actually +0x8, 64-bit)
    uint32_t m_size;       // +0x10
    uint32_t m_ctrl;       // +0x14  bit[0] = start

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void transfer_thread();

    uint32_t readWord(uint64_t addr);
    void writeWord(uint64_t addr, uint32_t data);
};

}  // namespace riscv_soc_tlm

#endif
