#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <cstdint>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"

namespace riscv_soc_tlm
{

class Display : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<Display> initiator_socket;

    Display(sc_core::sc_module_name name, uint64_t fb_base, uint32_t fb_width,
            uint32_t fb_height);

private:
    uint64_t m_fb_base;
    uint32_t m_width;
    uint32_t m_height;

    void refresh_thread();
    uint32_t readPixel(uint64_t addr);
};

}  // namespace riscv_soc_tlm

#endif
