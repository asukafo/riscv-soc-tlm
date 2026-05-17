#ifndef INTERCONNECT_H
#define INTERCONNECT_H

#include <cstdint>
#include <vector>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/multi_passthrough_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"

namespace rv32
{

class Interconnect : public sc_core::sc_module
{
   public:
    tlm_utils::multi_passthrough_target_socket<Interconnect> target_socket;
    tlm_utils::simple_initiator_socket<Interconnect> mem_socket;

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

}  // namespace rv32

#endif
