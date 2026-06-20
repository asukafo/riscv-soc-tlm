#include "periph/display/display.h"

#include <cassert>
#include <cstring>
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
    target_socket.register_transport_dbg(this, &Display::transport_dbg);
    // AT backward path for framebuffer reads
    initiator_socket.register_nb_transport_bw(this, &Display::nb_transport_bw);
    SC_THREAD(refresh_thread);
}

// ═══════════════════════════════════════════════════════════════════════
// LT path — MMIO register access
// ═══════════════════════════════════════════════════════════════════════
void Display::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();
    unsigned char* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();
    // Clause 16.4: accumulate, don't overwrite (quantum keeper)
    delay += sc_core::sc_time(1, sc_core::SC_NS);

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

// ═══════════════════════════════════════════════════════════════════════
// AT backward path — Clause 15.2.4, 15.2.5
// ═══════════════════════════════════════════════════════════════════════
tlm::tlm_sync_enum Display::nb_transport_bw(tlm::tlm_generic_payload& trans,
                                              tlm::tlm_phase& phase,
                                              sc_core::sc_time& delay)
{
    if (phase == tlm::END_REQ)
    {
        // Clause 15.2.4: request exclusion satisfied
        m_end_req_event.notify(sc_core::SC_ZERO_TIME);
        delay = sc_core::SC_ZERO_TIME;
        return tlm::TLM_ACCEPTED;
    }

    if (phase == tlm::BEGIN_RESP)
    {
        // Capture read pixel data on response
        std::memcpy(&m_read_data, trans.get_data_ptr(), 4);

        m_resp_event.notify(sc_core::SC_ZERO_TIME);
        phase = tlm::END_RESP;  // Response exclusion (Clause 15.2.5)
        delay = sc_core::SC_ZERO_TIME;
        return tlm::TLM_UPDATED;
    }

    return tlm::TLM_ACCEPTED;
}

// ═══════════════════════════════════════════════════════════════════════
// AT initiator — read one pixel via nb_transport_fw
// ═══════════════════════════════════════════════════════════════════════
uint32_t Display::readPixelAT(uint64_t addr)
{
    m_at_trans = m_mm->allocate();
    m_at_trans->set_command(tlm::TLM_READ_COMMAND);
    m_at_trans->set_address(addr);
    m_at_trans->set_data_ptr(reinterpret_cast<unsigned char*>(&m_read_data));
    m_at_trans->set_data_length(4);
    m_at_trans->set_streaming_width(4);
    m_at_trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    m_at_trans->acquire();

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    tlm::tlm_sync_enum rc = initiator_socket->nb_transport_fw(*m_at_trans, phase, delay);

    if (!(rc == tlm::TLM_UPDATED && phase == tlm::END_REQ))
    {
        sc_core::wait(m_end_req_event);
    }

    sc_core::wait(m_resp_event);
    uint32_t result = m_read_data;

    // Let PEQ process END_RESP before we release (Clause 14.6 ordering)
    sc_core::wait(1, sc_core::SC_NS);
    m_at_trans->release();
    m_at_trans = nullptr;

    return result;
}

// ═══════════════════════════════════════════════════════════════════════
// LT fallback for framebuffer reads
// ═══════════════════════════════════════════════════════════════════════
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

// ═══════════════════════════════════════════════════════════════════════
// Refresh thread — reads framebuffer (AT or LT) and outputs ASCII art
// ═══════════════════════════════════════════════════════════════════════
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
                    uint32_t pixel;
                    if (m_use_at && m_mm != nullptr)
                        pixel = readPixelAT(addr);
                    else
                        pixel = readPixel(addr);
                    char c = (pixel != 0) ? '#' : '.';
                    std::cout << c;
                }
                std::cout << std::endl;
            }
        }
        sc_core::wait(100, sc_core::SC_US);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Debug transport — Clause 11.4 (MMIO peek/poke)
// ═══════════════════════════════════════════════════════════════════════
unsigned int Display::transport_dbg(tlm::tlm_generic_payload& trans)
{
    sc_core::sc_time t = sc_core::SC_ZERO_TIME;
    b_transport(trans, t);
    return trans.get_data_length();
}

}  // namespace riscv_soc_tlm
