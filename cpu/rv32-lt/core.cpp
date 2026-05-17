#include "cpu/rv32-lt/core.h"

#include <iostream>

namespace riscv_soc_tlm
{

SC_HAS_PROCESS(CPU);

CPU::CPU(sc_core::sc_module_name name, uint32_t start_pc)
    : sc_core::sc_module(name),
      instr_socket("instr_socket"),
      data_socket("data_socket"),
      executor()
{
    regs.setPC(start_pc);

    executor.setContext(this);
    executor.setMem(&CPU::memRead, &CPU::memWrite);
    executor.setReg(&CPU::regRead, &CPU::regWrite);
    executor.setPC(&CPU::regGetPC, &CPU::regSetPC, &CPU::regIncPC);
    executor.setDump(&CPU::regDump);

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

// ─── Memory callbacks ──────────────────────────────────────────────

uint32_t CPU::memRead(void* ctx, uint64_t addr, int size)
{
    CPU* cpu = static_cast<CPU*>(ctx);
    uint32_t data = 0;
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(size);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    cpu->data_socket->b_transport(trans, delay);

    if (trans.is_response_error())
    {
        std::cerr << "Read memory error at 0x" << std::hex << addr << std::dec << std::endl;
        sc_core::sc_stop();
    }

    return data;
}

void CPU::memWrite(void* ctx, uint64_t addr, uint32_t data, int size)
{
    CPU* cpu = static_cast<CPU*>(ctx);
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(size);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    cpu->data_socket->b_transport(trans, delay);

    if (trans.is_response_error())
    {
        std::cerr << "Write memory error at 0x" << std::hex << addr << std::dec << std::endl;
        sc_core::sc_stop();
    }
}

// ─── Register callbacks ─────────────────────────────────────────────

uint32_t CPU::regRead(void* ctx, uint32_t reg_num)
{
    return static_cast<CPU*>(ctx)->regs.getValue(reg_num);
}

void CPU::regWrite(void* ctx, uint32_t reg_num, uint32_t value)
{
    static_cast<CPU*>(ctx)->regs.setValue(reg_num, value);
}

uint32_t CPU::regGetPC(void* ctx)
{
    return static_cast<CPU*>(ctx)->regs.getPC();
}

void CPU::regSetPC(void* ctx, uint32_t pc)
{
    static_cast<CPU*>(ctx)->regs.setPC(pc);
}

void CPU::regIncPC(void* ctx)
{
    static_cast<CPU*>(ctx)->regs.incPC();
}

void CPU::regDump(void* ctx)
{
    static_cast<CPU*>(ctx)->regs.dump();
}

}  // namespace riscv_soc_tlm
