#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <cstdint>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

namespace riscv_soc_tlm
{

class Display : public sc_core::sc_module
{
public:
    // CPU writes MMIO registers via this socket
    tlm_utils::simple_target_socket<Display> target_socket;
    // Display reads framebuffer via this socket
    tlm_utils::simple_initiator_socket<Display> initiator_socket;

    Display(sc_core::sc_module_name name);

private:
    // MMIO registers (offsets from base 0x10001000)
    uint64_t m_fb_addr;  // +0x00..0x07
    uint32_t m_width;    // +0x08
    uint32_t m_height;   // +0x0C
    uint32_t m_ctrl;     // +0x10  bit[0] = enable

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void refresh_thread();
    uint32_t readPixel(uint64_t addr);
};

}  // namespace riscv_soc_tlm

#endif
