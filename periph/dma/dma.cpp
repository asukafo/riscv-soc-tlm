#include "periph/dma/dma.h"

#include <iostream>

namespace riscv_soc_tlm
{

SC_HAS_PROCESS(DMA);

DMA::DMA(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      target_socket("target_socket"),
      initiator_socket("initiator_socket"),
      m_src_addr(0),
      m_dst_addr(0),
      m_size(0),
      m_ctrl(0)
{
    target_socket.register_b_transport(this, &DMA::b_transport);
    SC_THREAD(transfer_thread);
}

void DMA::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
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
                m_src_addr = (m_src_addr & 0xFFFFFFFF00000000ULL) |
                             (uint64_t(value) << ((addr & 0x04) ? 32 : 0));
                break;
            case 0x08:
            case 0x0C:
                m_dst_addr = (m_dst_addr & 0xFFFFFFFF00000000ULL) |
                             (uint64_t(value) << ((addr & 0x0C) ? 32 : 0));
                break;
            case 0x10:
                m_size = value;
                break;
            case 0x14:
                m_ctrl = value;
                break;
        }
    }
    else if (trans.get_command() == tlm::TLM_READ_COMMAND)
    {
        uint32_t value = 0;
        switch (addr & 0xFF)
        {
            case 0x00: value = static_cast<uint32_t>(m_src_addr); break;
            case 0x04: value = static_cast<uint32_t>(m_src_addr >> 32); break;
            case 0x08: value = static_cast<uint32_t>(m_dst_addr); break;
            case 0x0C: value = static_cast<uint32_t>(m_dst_addr >> 32); break;
            case 0x10: value = m_size; break;
            case 0x14: value = m_ctrl; break;
        }
        std::memcpy(ptr, &value, std::min(len, 4u));
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

uint32_t DMA::readWord(uint64_t addr)
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

void DMA::writeWord(uint64_t addr, uint32_t data)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
}

void DMA::transfer_thread()
{
    while (true)
    {
        if (m_ctrl & 1)
        {
            std::cout << "[DMA] Transfer " << m_size << " bytes from 0x" << std::hex
                      << m_src_addr << " to 0x" << m_dst_addr << std::dec << std::endl;

            for (uint32_t offset = 0; offset < m_size; offset += 4)
            {
                uint32_t data = readWord(m_src_addr + offset);
                writeWord(m_dst_addr + offset, data);
            }

            std::cout << "[DMA] Transfer complete." << std::endl;
            m_ctrl = 0; // clear start bit
        }
        sc_core::wait(100, sc_core::SC_NS);
    }
}

}  // namespace riscv_soc_tlm
