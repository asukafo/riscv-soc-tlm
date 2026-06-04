#ifndef __REGISTERS_H__
#define __REGISTERS_H__

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>

#include "cpu/rv32-lt/csr.h"

// RISC-V RV32I register file (32 x 32-bit, x0 hardwired to zero).
//
// Register  ABI Name  Description
// x0        zero      Hardwired zero
// x1        ra        Return address
// x2        sp        Stack pointer
// x3        gp        Global pointer
// x4        tp        Thread pointer
// x5-x7     t0-t2     Temporaries (caller-saved)
// x8        s0/fp     Saved register / Frame pointer
// x9        s1        Saved register
// x10-x11   a0-a1     Function arguments / return values
// x12-x17   a2-a7     Function arguments
// x18-x27   s2-s11    Saved registers (callee-saved)
// x28-x31   t3-t6     Temporaries (caller-saved)
//
// PC starts at 0x80000000, SP starts at top of 8MB memory (0x800000).

namespace riscv_soc_tlm
{

class Registers
{
public:
    CSR csr;  // machine-mode control and status registers

    Registers()
    {
        regs.fill(0);
        pc = 0x80000000;
        regs[2] = 0x800000;  // sp = stack pointer at top of 8MB memory
    }

    uint32_t getValue(uint32_t reg_num) const
    {
        if (reg_num < 32)
            return regs[reg_num];
        return 0xFFFFFFFF;
    }

    void setValue(uint32_t reg_num, uint32_t value)
    {
        if (reg_num != 0 && reg_num < 32)
        {
            regs[reg_num] = value;
        }
    }

    uint32_t getPC() const
    {
        return pc;
    }
    void setPC(uint32_t new_pc)
    {
        pc = new_pc;
    }
    void incPC()
    {
        pc += 4;
    }

    void dump() const
    {
        std::cout << "\n=== Register Dump ===" << std::endl;
        std::cout << "PC = 0x" << std::hex << pc << std::dec << std::endl;
        for (int i = 0; i < 32; i++)
        {
            std::cout << "x" << std::dec << std::setw(2) << i << " = 0x" << std::hex << std::setw(8)
                      << std::setfill('0') << regs[i] << std::setfill(' ') << std::dec;
            if ((i + 1) % 4 == 0)
                std::cout << std::endl;
            else
                std::cout << "  ";
        }
        std::cout << std::endl;
    }

private:
    std::array<uint32_t, 32> regs;
    uint32_t pc;
};

}  // namespace riscv_soc_tlm

#endif
