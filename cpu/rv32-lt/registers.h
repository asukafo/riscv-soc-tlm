#ifndef REGISTERS_H
#define REGISTERS_H

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace riscv_soc_tlm
{

class Registers
{
public:
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
