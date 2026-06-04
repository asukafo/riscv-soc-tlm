#include "periph/clint/clint.h"

#include <cstring>
#include <iostream>

namespace riscv_soc_tlm
{

// Bit masks for interrupt pending (must match cpu/rv32-lt/csr.h)
static constexpr uint32_t MIP_MSIP = 1u << 3;
static constexpr uint32_t MIP_MTIP = 1u << 7;

SC_HAS_PROCESS(CLINT);

CLINT::CLINT(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      target_socket("target_socket"),
      m_msip(0),
      m_mtimecmp(0),
      m_mtime(0)
{
    target_socket.register_b_transport(this, &CLINT::b_transport);
    SC_THREAD(tick_thread);
}

uint32_t CLINT::interrupt_pending() const
{
    uint32_t pending = 0;
    if (m_msip != 0)
        pending |= MIP_MSIP;
    if (m_mtime >= m_mtimecmp)
        pending |= MIP_MTIP;
    return pending;
}

void CLINT::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();
    unsigned char* data = trans.get_data_ptr();
    unsigned int len = trans.get_data_length();

    delay = sc_core::SC_ZERO_TIME;

    if (trans.get_command() == tlm::TLM_READ_COMMAND)
    {
        uint32_t value = 0;
        switch (addr & 0xFFFF)
        {
            case 0x0000:  value = m_msip;  break;
            case 0x4000:  value = static_cast<uint32_t>(m_mtimecmp & 0xFFFFFFFFu);  break;
            case 0x4004:  value = static_cast<uint32_t>(m_mtimecmp >> 32);           break;
            case 0xBFF8:  value = static_cast<uint32_t>(m_mtime & 0xFFFFFFFFu);     break;
            case 0xBFFC:  value = static_cast<uint32_t>(m_mtime >> 32);              break;
            default:
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
        }
        std::memcpy(data, &value, std::min(len, 4u));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    else if (trans.get_command() == tlm::TLM_WRITE_COMMAND)
    {
        uint32_t value = 0;
        std::memcpy(&value, data, std::min(len, 4u));

        switch (addr & 0xFFFF)
        {
            case 0x0000:  m_msip = value & 1;  break;
            case 0x4000:
                // Write lower 32 bits of mtimecmp
                m_mtimecmp = (m_mtimecmp & 0xFFFFFFFF00000000ull) | value;
                break;
            case 0x4004:
                // Write upper 32 bits of mtimecmp
                m_mtimecmp = (m_mtimecmp & 0x00000000FFFFFFFFull) | (static_cast<uint64_t>(value) << 32);
                break;
            case 0xBFF8:
                // Write lower 32 bits of mtime (for testing)
                m_mtime = (m_mtime & 0xFFFFFFFF00000000ull) | value;
                break;
            case 0xBFFC:
                // Write upper 32 bits of mtime (for testing)
                m_mtime = (m_mtime & 0x00000000FFFFFFFFull) | (static_cast<uint64_t>(value) << 32);
                break;
            default:
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
}

void CLINT::tick_thread()
{
    while (true)
    {
        sc_core::wait(10, sc_core::SC_NS);
        m_mtime++;
    }
}

}  // namespace riscv_soc_tlm
