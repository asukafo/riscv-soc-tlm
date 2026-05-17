#ifndef CPU_H
#define CPU_H

#include <cstdint>
#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "cpu/rv32-lt/registers.h"
#include "cpu/rv32-lt/memory_interface.h"
#include "cpu/rv32-lt/base_isa.h"

namespace rv32 {

class CPU : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<CPU> instr_socket;

    Registers regs;
    MemoryInterface mem_if;

    CPU(sc_core::sc_module_name name, uint32_t start_pc);

private:
    Executor executor;

    void CPU_thread();
    uint32_t fetchInstruction();
};

} // namespace rv32

#endif
