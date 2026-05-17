#include "periph/dma/dma.h"

#include <iostream>

namespace riscv_soc_tlm
{

DMA::DMA(sc_core::sc_module_name name)
    : sc_core::sc_module(name), initiator_socket("initiator_socket")
{
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

void DMA::transfer(uint64_t src, uint64_t dst, uint32_t size)
{
    std::cout << "[DMA] Transfer " << size << " bytes from 0x" << std::hex << src << " to 0x"
              << dst << std::dec << std::endl;

    for (uint32_t offset = 0; offset < size; offset += 4)
    {
        uint32_t data = readWord(src + offset);
        writeWord(dst + offset, data);
    }

    std::cout << "[DMA] Transfer complete." << std::endl;
}

}  // namespace riscv_soc_tlm
