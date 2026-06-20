#ifndef __DMA_H__
#define __DMA_H__

#include <cstdint>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm/soc_common.h"

namespace riscv_soc_tlm
{

class DMA : public sc_core::sc_module
{
public:
    // CPU writes MMIO registers via this socket (LT path)
    tlm_utils::simple_target_socket<DMA> target_socket;
    // DMA reads/writes memory via this socket (AT path)
    tlm_utils::simple_initiator_socket<DMA> initiator_socket;

    DMA(sc_core::sc_module_name name);

    void setMM(SoCMM* mm) { m_mm = mm; }

private:
    // MMIO registers (offsets from base 0x10000000)
    uint64_t m_src_addr;   // +0x00..0x07
    uint64_t m_dst_addr;   // +0x08..0x0F
    uint32_t m_size;       // +0x10
    uint32_t m_ctrl;       // +0x14  bit[0] = start

    // LT path for MMIO register access
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // AT initiator thread — Clause 15.2.4/15.2.5
    void transfer_thread();

    // AT backward path — receives END_REQ and BEGIN_RESP
    tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
                                        tlm::tlm_phase& phase,
                                        sc_core::sc_time& delay);

    // Debug transport — Clause 11.4
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    SoCMM* m_mm = nullptr;

    // AT transaction state — Clause 15.2.4 (request exclusion)
    tlm::tlm_generic_payload* m_at_trans = nullptr;
    sc_core::sc_event m_end_req_event;   // Clause 15.2.4: request exclusion
    sc_core::sc_event m_resp_event;      // BEGIN_RESP received
    uint32_t m_read_data = 0;   // buffer for read response data
    bool m_write_done = false;   // set when write response received

    // Per-word AT helpers
    uint32_t readWordAT(uint64_t addr);
    void writeWordAT(uint64_t addr, uint32_t data);
};

}  // namespace riscv_soc_tlm

#endif
