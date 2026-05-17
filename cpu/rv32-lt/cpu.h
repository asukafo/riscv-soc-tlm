#ifndef CPU_H
#define CPU_H

#include <cstdint>

#include "cpu/rv32-lt/base_isa.h"
#include "cpu/rv32-lt/memory_interface.h"
#include "cpu/rv32-lt/registers.h"
#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"

namespace riscv_soc_tlm
{

class CPU : public sc_core::sc_module
{
public:
    Registers regs;
    MemoryInterface mem_if;

    CPU(sc_core::sc_module_name name, uint32_t start_pc);

private:
    Executor executor;

    void CPU_thread();
};

}  // namespace riscv_soc_tlm

#endif
