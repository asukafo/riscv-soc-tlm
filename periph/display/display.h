#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <cstdint>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm/soc_common.h"

namespace riscv_soc_tlm
{

class Display : public sc_core::sc_module
{
public:
    // CPU writes MMIO registers via this socket (LT path)
    tlm_utils::simple_target_socket<Display> target_socket;
    // Display reads framebuffer via this socket (AT path)
    tlm_utils::simple_initiator_socket<Display> initiator_socket;

    Display(sc_core::sc_module_name name);

    void setMM(SoCMM* mm) { m_mm = mm; }

private:
    // MMIO registers (offsets from base 0x10001000)
    uint64_t m_fb_addr;  // +0x00..0x07
    uint32_t m_width;    // +0x08
    uint32_t m_height;   // +0x0C
    uint32_t m_ctrl;     // +0x10  bit[0] = enable

    // LT path for MMIO register access
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void refresh_thread();

    // AT path for framebuffer reads (multi-initiator verification)
    tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
                                        tlm::tlm_phase& phase,
                                        sc_core::sc_time& delay);
    uint32_t readPixelAT(uint64_t addr);

    // LT fallback for framebuffer reads
    uint32_t readPixel(uint64_t addr);

    // Debug transport — Clause 11.4
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    SoCMM* m_mm = nullptr;
    bool m_use_at = true;  // true = AT path, false = LT fallback

    // AT transaction state — Clause 15.2.4/15.2.5
    tlm::tlm_generic_payload* m_at_trans = nullptr;
    sc_core::sc_event m_end_req_event;
    sc_core::sc_event m_resp_event;
    uint32_t m_read_data = 0;
};

}  // namespace riscv_soc_tlm

#endif
