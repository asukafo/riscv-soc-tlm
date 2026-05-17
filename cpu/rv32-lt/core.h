#ifndef __CPU_H__
#define __CPU_H__

#include <cstdint>

#include "cpu/rv32-lt/base_isa.h"
#include "cpu/rv32-lt/registers.h"
#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"

namespace riscv_soc_tlm
{

class CPU : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<CPU> instr_socket;
    tlm_utils::simple_initiator_socket<CPU> data_socket;
    Registers regs;

    CPU(sc_core::sc_module_name name, uint32_t start_pc);

private:
    Executor executor;

    void CPU_thread();
    uint32_t fetchInstruction();

    // Callbacks for Executor — memory access
    static uint32_t memRead(void* ctx, uint64_t addr, int size);
    static void memWrite(void* ctx, uint64_t addr, uint32_t data, int size);

    // Callbacks for Executor — register access
    static uint32_t regRead(void* ctx, uint32_t reg_num);
    static void regWrite(void* ctx, uint32_t reg_num, uint32_t value);
    static uint32_t regGetPC(void* ctx);
    static void regSetPC(void* ctx, uint32_t pc);
    static void regIncPC(void* ctx);
    static void regDump(void* ctx);
};

}  // namespace riscv_soc_tlm

#endif
