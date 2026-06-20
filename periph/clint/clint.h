#ifndef __CLINT_H__
#define __CLINT_H__

#include <cstdint>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"

namespace riscv_soc_tlm
{

// CLINT: Core-Local Interruptor (RISC-V platform-level interrupt controller).
//
// Memory-mapped at 0x02000000:
//   +0x0000  msip       (R/W, 4B)  machine software interrupt pending
//   +0x4000  mtimecmp   (R/W, 8B)  machine timer compare (lo at +0x4000, hi at +0x4004)
//   +0xBFF8  mtime      (R/W, 8B)  machine time counter   (lo at +0xBFF8, hi at +0xBFFC)
//
// The SC_THREAD tick_thread() increments mtime every clock tick.
// interrupt_pending() is polled by the CPU before each instruction.

class CLINT : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<CLINT> target_socket;

    CLINT(sc_core::sc_module_name name);

    // Called by CPU to read pending interrupt bits (MSIP, MTIP)
    uint32_t interrupt_pending() const;

private:
    uint32_t m_msip;       // offset 0x0000
    uint64_t m_mtimecmp;   // offset 0x4000
    uint64_t m_mtime;      // offset 0xBFF8

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void tick_thread();

    // Debug transport — Clause 11.4
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
};

}  // namespace riscv_soc_tlm

#endif
