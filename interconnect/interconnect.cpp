#include "interconnect/interconnect.h"
#include <iostream>

namespace rv32 {

Interconnect::Interconnect(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      target_socket("target_socket"),
      mem_socket("mem_socket")
{
    target_socket.register_b_transport(this, &Interconnect::b_transport);
}

void Interconnect::map(uint64_t base, uint64_t size,
                       tlm_utils::simple_initiator_socket<Interconnect>& socket)
{
    regions.push_back({base, size, &socket});
}

void Interconnect::b_transport(int /*id*/, tlm::tlm_generic_payload& trans,
                               sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();

    for (auto& region : regions) {
        if (addr >= region.base && addr < region.base + region.size) {
            (*region.socket)->b_transport(trans, delay);
            return;
        }
    }

    std::cerr << "Interconnect: no target at address 0x"
              << std::hex << addr << std::dec << std::endl;
    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
}

} // namespace rv32
