#include "cpu/rv32-lt/memory_interface.h"
#include <iostream>
#include <sstream>

namespace rv32 {

MemoryInterface::MemoryInterface()
    : data_socket("data_socket") {}

uint32_t MemoryInterface::readDataMem(uint64_t addr, int size) {
    uint32_t data = 0;
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(size);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    trans.set_address(addr);

    data_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        std::stringstream ss;
        ss << "Read memory error: 0x" << std::hex << addr;
        SC_REPORT_ERROR("MemoryInterface", ss.str().c_str());
    }

    return data;
}

void MemoryInterface::writeDataMem(uint64_t addr, uint32_t data, int size) {
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(size);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    trans.set_address(addr);

    data_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        std::stringstream ss;
        ss << "Write memory error: 0x" << std::hex << addr;
        SC_REPORT_ERROR("MemoryInterface", ss.str().c_str());
    }
}

} // namespace rv32
