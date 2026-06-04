#include "cpu/rv32-lt/core.h"
#include "periph/clint/clint.h"

#include <iostream>

namespace riscv_soc_tlm
{

SC_HAS_PROCESS(CPU);

CPU::CPU(sc_core::sc_module_name name, uint32_t start_pc)
    : sc_core::sc_module(name),
      instr_socket("instr_socket"),
      data_socket("data_socket"),
      executor(),
      m_clint(nullptr)
{
    regs.setPC(start_pc);

    executor.setContext(this);
    executor.setMem(&CPU::memRead, &CPU::memWrite);
    executor.setReg(&CPU::regRead, &CPU::regWrite);
    executor.setPC(&CPU::regGetPC, &CPU::regSetPC, &CPU::regIncPC);
    executor.setDump(&CPU::regDump);
    executor.setCSR(&CPU::csrRead, &CPU::csrWrite);

    SC_THREAD(CPU_thread);
}

void CPU::CPU_thread()
{
    while (true)
    {
        checkInterrupts();

        uint32_t instr_raw = fetchInstruction();
        bool pc_updated = executor.execute(instr_raw);

        if (!pc_updated)
        {
            regs.incPC();
        }

        sc_core::wait(10, sc_core::SC_NS);
    }
}

void CPU::checkInterrupts()
{
    if (m_clint == nullptr)
        return;

    // Sync external interrupt pending from CLINT
    uint32_t ext_pending = m_clint->interrupt_pending();
    regs.csr.update_external_ip(ext_pending);

    // Check if any enabled interrupt is pending and global MIE is set
    uint32_t mip = regs.csr.read(CSR::ADDR_MIP);
    uint32_t mie = regs.csr.read(CSR::ADDR_MIE);
    uint32_t mstatus = regs.csr.read(CSR::ADDR_MSTATUS);

    if (!(mstatus & CSR::MSTATUS_MIE))
        return;

    uint32_t active = mip & mie;
    if (active == 0)
        return;

    // Priority: MEI > MSI > MTI
    uint32_t cause;
    if (active & CSR::MEIE)
        cause = CSR::IRQ_MEI;
    else if (active & CSR::MSIP)
        cause = CSR::IRQ_MSI;
    else if (active & CSR::MTIP)
        cause = CSR::IRQ_MTI;
    else
        return;

    // Take the trap
    regs.csr.take_trap(regs.getPC(), cause, true);

    // Set PC to trap handler from mtvec
    uint32_t mtvec = regs.csr.read(CSR::ADDR_MTVEC);
    uint32_t mode = mtvec & 0x3;
    uint32_t base = mtvec & ~0x3u;
    if (mode == 1)  // vectored
        regs.setPC(base + 4 * (cause & 0x1F));
    else  // direct
        regs.setPC(base);
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

// ─── CSR callbacks ──────────────────────────────────────────────────

uint32_t CPU::csrRead(void* ctx, uint32_t addr)
{
    return static_cast<CPU*>(ctx)->regs.csr.read(addr);
}

void CPU::csrWrite(void* ctx, uint32_t addr, uint32_t value)
{
    static_cast<CPU*>(ctx)->regs.csr.write(addr, value);
}

}  // namespace riscv_soc_tlm
