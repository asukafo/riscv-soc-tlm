#ifndef __FPU_H__
#define __FPU_H__

#include <cstdint>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"

namespace riscv_soc_tlm
{

// FPU: IEEE 754 single-precision floating-point coprocessor
//
// MMIO register map (base 0x10002000):
//   +0x00  OPERAND_A   (R/W)  first operand (32-bit float bit pattern)
//   +0x04  OPERAND_B   (R/W)  second operand
//   +0x08  OPERATION   (R/W)  operation code (0x00..0x0C)
//   +0x0C  RESULT      (R)    result (float or int bit pattern)
//   +0x10  STATUS      (R)    bit0=busy, bit1=done, bit2=exception, [7:3]=cause
//   +0x14  CTRL        (R/W)  bit0=start (auto-clear), bit1=clear exception
//   +0x18  OPERAND_C   (R/W)  third operand (for FMADD)

class FPU : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<FPU> target_socket;

    FPU(sc_core::sc_module_name name);

private:
    uint32_t m_op_a;
    uint32_t m_op_b;
    uint32_t m_op_c;
    uint32_t m_operation;
    uint32_t m_result;
    uint32_t m_status;
    uint32_t m_ctrl;

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void compute_thread();
};

}  // namespace riscv_soc_tlm

#endif
