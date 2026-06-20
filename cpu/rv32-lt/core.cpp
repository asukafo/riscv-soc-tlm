#include "cpu/rv32-lt/core.h"
#include "periph/clint/clint.h"

#include <cstring>
#include <iostream>

namespace riscv_soc_tlm
{

SC_HAS_PROCESS(CPU);

CPU::CPU(sc_core::sc_module_name name, uint32_t start_pc)
    : sc_core::sc_module(name),
      instr_socket("instr_socket"),
      data_socket("data_socket"),
      qk(),  // Clause 16.4: default-constructed, reset() in body
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

    // Clause 11.3: register DMI invalidation callback on both sockets
    instr_socket.register_invalidate_direct_mem_ptr(this, &CPU::invalidate_direct_mem_ptr);
    data_socket.register_invalidate_direct_mem_ptr(this, &CPU::invalidate_direct_mem_ptr);

    // Clause 16.4: quantum keeper starts at local time zero
    qk.reset();

    SC_THREAD(CPU_thread);
}

// ═══════════════════════════════════════════════════════════════════════
// DMI cache — Clause 11.3
// ═══════════════════════════════════════════════════════════════════════

CPU::DmiCache* CPU::find_dmi(uint64_t addr)
{
    for (auto& d : dmi_cache)
    {
        if (addr >= d.base && addr <= d.end)
            return &d;
    }
    return nullptr;
}

void CPU::add_dmi(uint64_t addr, tlm_utils::simple_initiator_socket<CPU>& sock)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(addr);
    trans.set_read();

    tlm::tlm_dmi dmi_data;
    if (sock->get_direct_mem_ptr(trans, dmi_data))
    {
        DmiCache d;
        d.base          = dmi_data.get_start_address();
        d.end           = dmi_data.get_end_address();
        d.ptr           = dmi_data.get_dmi_ptr();
        d.read_latency  = dmi_data.get_read_latency();
        d.write_latency = dmi_data.get_write_latency();
        d.can_read      = dmi_data.is_read_allowed();
        d.can_write     = dmi_data.is_write_allowed();
        dmi_cache.push_back(d);
    }
}

void CPU::invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end)
{
    // Clause 11.3.6: remove any cached regions that overlap [start, end]
    dmi_cache.erase(
        std::remove_if(dmi_cache.begin(), dmi_cache.end(),
                       [start, end](const DmiCache& d) {
                           return !(d.end < start || d.base > end);
                       }),
        dmi_cache.end());
}

// ═══════════════════════════════════════════════════════════════════════
// CPU thread — Clause 16.4: temporal decoupling via quantum keeper
// ═══════════════════════════════════════════════════════════════════════
void CPU::CPU_thread()
{
    while (true)
    {
        checkInterrupts();

        uint32_t instr_raw = fetchInstruction();
        bool pc_updated = executor.execute(instr_raw);

        if (executor.stopped())
        {
            // ECALL was executed; force sync so scheduler terminates
            qk.sync();
            break;
        }

        if (!pc_updated)
        {
            regs.incPC();
        }

        // Clause 16.4: accumulate local time; sync when quantum exceeded
        qk.inc(sc_core::sc_time(10, sc_core::SC_NS));
        if (qk.need_sync())
            qk.sync();
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

// ═══════════════════════════════════════════════════════════════════════
// Instruction fetch — with DMI + quantum keeper
// ═══════════════════════════════════════════════════════════════════════
uint32_t CPU::fetchInstruction()
{
    uint64_t addr = regs.getPC();
    uint32_t data = 0;

    // Clause 11.3: try DMI cache first
    DmiCache* d = find_dmi(addr);
    if (d && d->can_read)
    {
        std::memcpy(&data, d->ptr + (addr - d->base), 4);
        qk.inc(d->read_latency);
        return data;
    }

    // LT path via b_transport
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time delay = qk.get_local_time();  // Clause 16.4
    instr_socket->b_transport(trans, delay);
    qk.set(delay);                                  // Clause 16.4

    if (trans.is_response_error())
    {
        std::cerr << "Fetch error at PC=0x" << std::hex << regs.getPC()
                  << std::dec << std::endl;
        sc_core::sc_stop();
    }

    // Clause 11.3: establish DMI if allowed
    if (trans.is_dmi_allowed())
    {
        add_dmi(addr, instr_socket);
    }

    return data;
}

// ─── Memory callbacks — with DMI + quantum keeper ──────────────────
static bool is_mmio_addr(uint64_t addr)
{
    return (addr >= 0x10000000 && addr < 0x10003000) ||  // DMA + Display + FPU
           (addr >= 0x02000000 && addr < 0x02010000);     // CLINT
}

bool CPU::is_mmio(uint64_t addr) { return is_mmio_addr(addr); }

uint32_t CPU::memRead(void* ctx, uint64_t addr, int size)
{
    CPU* cpu = static_cast<CPU*>(ctx);
    uint32_t data = 0;

    // Clause 11.3: try DMI cache first
    DmiCache* d = cpu->find_dmi(addr);
    if (d && d->can_read)
    {
        std::memcpy(&data, d->ptr + (addr - d->base), size);
        cpu->qk.inc(d->read_latency);
        return data;
    }

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(size);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time delay = cpu->qk.get_local_time();
    cpu->data_socket->b_transport(trans, delay);
    cpu->qk.set(delay);

    if (trans.is_response_error())
    {
        std::cerr << "Read memory error at 0x" << std::hex << addr
                  << std::dec << std::endl;
        sc_core::sc_stop();
    }

    if (trans.is_dmi_allowed())
    {
        cpu->add_dmi(addr, cpu->data_socket);
    }

    // MMIO access: force sync so peripherals (DMA) can make progress
    if (is_mmio_addr(addr))
        cpu->qk.sync();

    return data;
}

void CPU::memWrite(void* ctx, uint64_t addr, uint32_t data, int size)
{
    CPU* cpu = static_cast<CPU*>(ctx);

    // Clause 11.3: try DMI cache first
    DmiCache* d = cpu->find_dmi(addr);
    if (d && d->can_write)
    {
        std::memcpy(d->ptr + (addr - d->base), &data, size);
        cpu->qk.inc(d->write_latency);
        return;
    }

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(size);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time delay = cpu->qk.get_local_time();
    cpu->data_socket->b_transport(trans, delay);
    cpu->qk.set(delay);

    if (trans.is_response_error())
    {
        std::cerr << "Write memory error at 0x" << std::hex << addr
                  << std::dec << std::endl;
        sc_core::sc_stop();
    }

    if (trans.is_dmi_allowed())
    {
        cpu->add_dmi(addr, cpu->data_socket);
    }

    // MMIO access: force sync so peripherals (DMA) can make progress
    if (is_mmio_addr(addr))
        cpu->qk.sync();
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
