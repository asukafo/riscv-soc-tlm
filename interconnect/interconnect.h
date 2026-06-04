#ifndef __INTERCONNECT_H__
#define __INTERCONNECT_H__

#include <cstdint>
#include <vector>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/multi_passthrough_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"

namespace riscv_soc_tlm
{

class Interconnect : public sc_core::sc_module
{
public:
    tlm_utils::multi_passthrough_target_socket<Interconnect> target_socket;
    tlm_utils::simple_initiator_socket<Interconnect> mem_socket;
    tlm_utils::simple_initiator_socket<Interconnect> dma_mmio_socket;
    tlm_utils::simple_initiator_socket<Interconnect> display_mmio_socket;
    tlm_utils::simple_initiator_socket<Interconnect> fpu_mmio_socket;

    Interconnect(sc_core::sc_module_name name);

    void map(uint64_t base, uint64_t size,
             tlm_utils::simple_initiator_socket<Interconnect>& socket);

private:
    struct AddressRegion
    {
        uint64_t base;
        uint64_t size;
        tlm_utils::simple_initiator_socket<Interconnect>* socket;
    };
    std::vector<AddressRegion> regions;

    void b_transport(int id, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace riscv_soc_tlm

#endif
