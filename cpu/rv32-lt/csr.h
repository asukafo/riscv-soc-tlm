#ifndef __CSR_H__
#define __CSR_H__

#include <cstdint>

namespace riscv_soc_tlm
{

// RISC-V machine-mode Control and Status Registers (CSR).
//
// Implements the minimum set needed for interrupt handling in M-mode:
//   mstatus  (0x300) — global interrupt enable + previous state
//   mtvec    (0x305) — trap handler base address + mode
//   mepc     (0x341) — exception program counter
//   mcause   (0x342) — trap cause (interrupt bit + exception code)
//   mie      (0x304) — interrupt enable (per-source)
//   mip      (0x344) — interrupt pending (per-source)
//   mscratch (0x340) — scratch register for trap handlers
//   mhartid  (0xF14) — hardware thread ID (hardwired to 0)

class CSR
{
public:
    // ── CSR addresses ─────────────────────────────────────────────────
    static constexpr uint32_t ADDR_MSTATUS  = 0x300;
    static constexpr uint32_t ADDR_MTVEC    = 0x305;
    static constexpr uint32_t ADDR_MEPC     = 0x341;
    static constexpr uint32_t ADDR_MCAUSE   = 0x342;
    static constexpr uint32_t ADDR_MIE      = 0x304;
    static constexpr uint32_t ADDR_MIP      = 0x344;
    static constexpr uint32_t ADDR_MSCRATCH = 0x340;
    static constexpr uint32_t ADDR_MHARTID  = 0xF14;

    // ── mstatus bit masks ─────────────────────────────────────────────
    static constexpr uint32_t MSTATUS_MIE  = 1u << 3;   // machine interrupt enable
    static constexpr uint32_t MSTATUS_MPIE = 1u << 7;   // previous MIE
    static constexpr uint32_t MSTATUS_MPP  = 3u << 11;  // previous privilege (11 = M-mode)

    // ── mie / mip bit masks ───────────────────────────────────────────
    static constexpr uint32_t MSIE = 1u << 3;   // machine software interrupt
    static constexpr uint32_t MTIE = 1u << 7;   // machine timer interrupt
    static constexpr uint32_t MEIE = 1u << 11;  // machine external interrupt

    static constexpr uint32_t MSIP = 1u << 3;   // mip bit 3
    static constexpr uint32_t MTIP = 1u << 7;   // mip bit 7
    static constexpr uint32_t MEIP = 1u << 11;  // mip bit 11

    // ── mcause codes ──────────────────────────────────────────────────
    static constexpr uint32_t CAUSE_ECALL_M = 11;   // environment call from M-mode
    static constexpr uint32_t IRQ_MSI       = 3;    // machine software interrupt
    static constexpr uint32_t IRQ_MTI       = 7;    // machine timer interrupt
    static constexpr uint32_t IRQ_MEI       = 11;   // machine external interrupt
    static constexpr uint32_t MCAUSE_INTR   = 1u << 31;  // interrupt flag in mcause

    CSR()
        : m_mstatus(0), m_mtvec(0), m_mepc(0), m_mcause(0),
          m_mie(0), m_mip(0), m_mscratch(0)
    {
    }

    // ── CSR read ──────────────────────────────────────────────────────
    uint32_t read(uint32_t addr) const
    {
        switch (addr)
        {
            case ADDR_MSTATUS:  return m_mstatus;
            case ADDR_MTVEC:    return m_mtvec;
            case ADDR_MEPC:     return m_mepc;
            case ADDR_MCAUSE:   return m_mcause;
            case ADDR_MIE:      return m_mie;
            case ADDR_MIP:      return m_mip;
            case ADDR_MSCRATCH: return m_mscratch;
            case ADDR_MHARTID:  return 0;  // single hart, always 0
            default:            return 0;
        }
    }

    // ── CSR write ─────────────────────────────────────────────────────
    void write(uint32_t addr, uint32_t value)
    {
        switch (addr)
        {
            case ADDR_MSTATUS:
                // Only MIE(3), MPIE(7), MPP(11:12) are writable
                m_mstatus = (m_mstatus & ~(MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP))
                          | (value & (MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_MPP));
                break;
            case ADDR_MTVEC:
                // MODE bits 1:0: only 0 (direct) and 1 (vectored) valid
                m_mtvec = value;
                break;
            case ADDR_MEPC:
                m_mepc = value;
                break;
            case ADDR_MCAUSE:
                // read-only: hardware sets this on trap
                break;
            case ADDR_MIE:
                // Only MSIE(3), MTIE(7), MEIE(11)
                m_mie = (m_mie & ~(MSIE | MTIE | MEIE))
                      | (value & (MSIE | MTIE | MEIE));
                break;
            case ADDR_MIP:
                // Only MSIP(3) is writable in mip
                m_mip = (m_mip & ~MSIP) | (value & MSIP);
                break;
            case ADDR_MSCRATCH:
                m_mscratch = value;
                break;
            case ADDR_MHARTID:
                // read-only
                break;
            default:
                break;
        }
    }

    // ── Update external interrupt pending from CLINT ──────────────────
    // Called by CPU::checkInterrupts() each cycle to sync MTIP and MSIP
    // from the CLINT peripheral.
    void update_external_ip(uint32_t ext_pending)
    {
        // Overwrite MSIP(3) and MTIP(7) from external source (CLINT)
        // MEIP(11) reserved for future PLIC
        m_mip = (m_mip & ~(MSIP | MTIP | MEIP))
              | (ext_pending & (MSIP | MTIP | MEIP));
    }

    // ── Take a trap (interrupt or exception) ─────────────────────────
    // Saves current PC to mepc, sets mcause, updates mstatus.
    void take_trap(uint32_t pc, uint32_t cause_code, bool is_interrupt)
    {
        m_mepc   = pc;
        m_mcause = (is_interrupt ? MCAUSE_INTR : 0) | (cause_code & 0x1F);

        // Save MIE → MPIE, clear MIE, set MPP = M-mode (3)
        uint32_t mpie = (m_mstatus & MSTATUS_MIE) ? MSTATUS_MPIE : 0;
        uint32_t mpp  = 3u << 11;  // previous privilege = M-mode
        m_mstatus = (m_mstatus & ~(MSTATUS_MPIE | MSTATUS_MIE | MSTATUS_MPP))
                  | mpie | mpp;
    }

    // ── Return from trap (MRET) ──────────────────────────────────────
    // Restores PC from mepc and updates mstatus.
    void mret(uint32_t& pc)
    {
        pc = m_mepc;

        // Restore MIE from MPIE, set MPIE = 1
        uint32_t mie = (m_mstatus & MSTATUS_MPIE) ? MSTATUS_MIE : 0;
        m_mstatus = (m_mstatus & ~(MSTATUS_MPIE | MSTATUS_MIE | MSTATUS_MPP))
                  | mie | MSTATUS_MPIE;
    }

private:
    uint32_t m_mstatus;
    uint32_t m_mtvec;
    uint32_t m_mepc;
    uint32_t m_mcause;
    uint32_t m_mie;
    uint32_t m_mip;
    uint32_t m_mscratch;
};

}  // namespace riscv_soc_tlm

#endif
