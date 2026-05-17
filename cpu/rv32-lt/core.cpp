#include "cpu/rv32-lt/core.h"

#include <iostream>

namespace riscv_soc_tlm
{

SC_HAS_PROCESS(CPU);

CPU::CPU(sc_core::sc_module_name name, uint32_t start_pc)
    : sc_core::sc_module(name), instr_socket("instr_socket"), mem_if(), executor(&regs, &mem_if)
{
    regs.setPC(start_pc);
    SC_THREAD(CPU_thread);
}

void CPU::CPU_thread()
{
    while (true)
    {
        uint32_t instr_raw = fetchInstruction();
        bool pc_updated = executor.execute(instr_raw);

        if (!pc_updated)
        {
            regs.incPC();
        }

        sc_core::wait(10, sc_core::SC_NS);
    }
}

uint32_t CPU::fetchInstruction()
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    uint32_t data = 0;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(regs.getPC());
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    instr_socket->b_transport(trans, delay);

    if (trans.is_response_error())
    {
        std::cerr << "Fetch error at PC=0x" << std::hex << regs.getPC() << std::dec
                  << std::endl;
        sc_core::sc_stop();
    }

    return data;
}

}  // namespace riscv_soc_tlm
