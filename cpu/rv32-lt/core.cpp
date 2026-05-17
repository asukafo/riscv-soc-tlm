#include "cpu/rv32-lt/core.h"

#include <iostream>

namespace riscv_soc_tlm
{

SC_HAS_PROCESS(CPU);

CPU::CPU(sc_core::sc_module_name name, uint32_t start_pc)
    : sc_core::sc_module(name), mem_if(), executor(&regs, &mem_if)
{
    regs.setPC(start_pc);
    SC_THREAD(CPU_thread);
}

void CPU::CPU_thread()
{
    while (true)
    {
        uint32_t instr_raw = mem_if.fetchInstruction(regs.getPC());
        bool pc_updated = executor.execute(instr_raw);

        if (!pc_updated)
        {
            regs.incPC();
        }

        sc_core::wait(10, sc_core::SC_NS);
    }
}

}  // namespace riscv_soc_tlm
