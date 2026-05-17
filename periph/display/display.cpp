#include "periph/display/display.h"

#include <iostream>

namespace riscv_soc_tlm
{

SC_HAS_PROCESS(Display);

Display::Display(sc_core::sc_module_name name, uint64_t fb_base, uint32_t fb_width,
                 uint32_t fb_height)
    : sc_core::sc_module(name),
      initiator_socket("initiator_socket"),
      m_fb_base(fb_base),
      m_width(fb_width),
      m_height(fb_height)
{
    SC_THREAD(refresh_thread);
}

uint32_t Display::readPixel(uint64_t addr)
{
    uint32_t data = 0;
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    return data;
}

void Display::refresh_thread()
{
    while (true)
    {
        std::cout << "\n[Display] Refresh:" << std::endl;
        for (uint32_t y = 0; y < m_height; y++)
        {
            for (uint32_t x = 0; x < m_width; x++)
            {
                uint64_t addr = m_fb_base + (y * m_width + x) * 4;
                uint32_t pixel = readPixel(addr);
                char c = (pixel != 0) ? '#' : '.';
                std::cout << c;
            }
            std::cout << std::endl;
        }
        sc_core::wait(100, sc_core::SC_US);
    }
}

}  // namespace riscv_soc_tlm
