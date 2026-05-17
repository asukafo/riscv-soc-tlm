#include "periph/display/display.h"

#include <iostream>

namespace riscv_soc_tlm
{

SC_HAS_PROCESS(Display);

Display::Display(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      target_socket("target_socket"),
      initiator_socket("initiator_socket"),
      m_fb_addr(0),
      m_width(16),
      m_height(16),
      m_ctrl(0)
{
    target_socket.register_b_transport(this, &Display::b_transport);
    SC_THREAD(refresh_thread);
}

void Display::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();
    unsigned char* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();
    delay = sc_core::SC_ZERO_TIME;

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND)
    {
        uint32_t value = 0;
        std::memcpy(&value, ptr, std::min(len, 4u));

        switch (addr & 0xFF)
        {
            case 0x00:
            case 0x04:
                m_fb_addr = (m_fb_addr & 0xFFFFFFFF00000000ULL) |
                            (uint64_t(value) << ((addr & 0x04) ? 32 : 0));
                break;
            case 0x08: m_width = value; break;
            case 0x0C: m_height = value; break;
            case 0x10: m_ctrl = value; break;
        }
    }
    else if (trans.get_command() == tlm::TLM_READ_COMMAND)
    {
        uint32_t value = 0;
        switch (addr & 0xFF)
        {
            case 0x00: value = static_cast<uint32_t>(m_fb_addr); break;
            case 0x04: value = static_cast<uint32_t>(m_fb_addr >> 32); break;
            case 0x08: value = m_width; break;
            case 0x0C: value = m_height; break;
            case 0x10: value = m_ctrl; break;
        }
        std::memcpy(ptr, &value, std::min(len, 4u));
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
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
        if (m_ctrl & 1)
        {
            std::cout << "\n[Display] Refresh:" << std::endl;
            for (uint32_t y = 0; y < m_height; y++)
            {
                for (uint32_t x = 0; x < m_width; x++)
                {
                    uint64_t addr = m_fb_addr + (y * m_width + x) * 4;
                    uint32_t pixel = readPixel(addr);
                    char c = (pixel != 0) ? '#' : '.';
                    std::cout << c;
                }
                std::cout << std::endl;
            }
        }
        sc_core::wait(100, sc_core::SC_US);
    }
}

}  // namespace riscv_soc_tlm
