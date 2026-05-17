#ifndef BASE_ISA_H
#define BASE_ISA_H

#include <cstdint>
#include <iostream>

#include "cpu/rv32-lt/memory_interface.h"
#include "cpu/rv32-lt/registers.h"

namespace riscv_soc_tlm
{

class Instruction
{
public:
    Instruction(uint32_t instr) : m_instr(instr)
    {
    }

    uint32_t opcode() const
    {
        return m_instr & 0x7F;
    }
    uint32_t rd() const
    {
        return (m_instr >> 7) & 0x1F;
    }
    uint32_t funct3() const
    {
        return (m_instr >> 12) & 0x7;
    }
    uint32_t rs1() const
    {
        return (m_instr >> 15) & 0x1F;
    }
    uint32_t rs2() const
    {
        return (m_instr >> 20) & 0x1F;
    }
    uint32_t funct7() const
    {
        return (m_instr >> 25) & 0x7F;
    }
    uint32_t getInstr() const
    {
        return m_instr;
    }

    int32_t imm_I() const
    {
        return static_cast<int32_t>(m_instr) >> 20;
    }

    int32_t imm_S() const
    {
        int32_t val = (static_cast<int32_t>(m_instr) >> 25) & 0x7F;
        val |= static_cast<int32_t>(m_instr >> 7) & 0x1F; /* set bottom 5 bits */
        return (val << 20) >> 20;                         /* sign-extend from bit 11 */
    }

    int32_t imm_B() const
    {
        int32_t val = 0;
        val |= ((m_instr >> 8) & 0xF) << 1;
        val |= ((m_instr >> 25) & 0x3F) << 5;
        val |= ((m_instr >> 7) & 0x1) << 11;
        val |= ((m_instr >> 31) & 0x1) << 12;
        return (val << 19) >> 19; /* sign-extend from bit 12 */
    }

    uint32_t imm_U() const
    {
        return m_instr & 0xFFFFF000;
    }

    int32_t imm_J() const
    {
        int32_t val = 0;
        val |= ((m_instr >> 21) & 0x3FF) << 1;
        val |= ((m_instr >> 20) & 0x1) << 11;
        val |= ((m_instr >> 12) & 0xFF) << 12;
        val |= ((m_instr >> 31) & 0x1) << 20;
        return (val << 11) >> 11; /* sign-extend from bit 20 */
    }

    uint32_t shamt() const
    {
        return (m_instr >> 20) & 0x1F;
    }

private:
    uint32_t m_instr;
};

class Executor
{
public:
    Executor(Registers* r, MemoryInterface* m) : regs(r), mem_if(m)
    {
    }

    bool execute(uint32_t instr_raw);

private:
    Registers* regs;
    MemoryInterface* mem_if;

    void LUI(const Instruction& i);
    void AUIPC(const Instruction& i);
    bool JAL(const Instruction& i);
    bool JALR(const Instruction& i);

    bool BEQ(const Instruction& i);
    bool BNE(const Instruction& i);
    bool BLT(const Instruction& i);
    bool BGE(const Instruction& i);
    bool BLTU(const Instruction& i);
    bool BGEU(const Instruction& i);

    void LB(const Instruction& i);
    void LH(const Instruction& i);
    void LW(const Instruction& i);
    void LBU(const Instruction& i);
    void LHU(const Instruction& i);

    void SB(const Instruction& i);
    void SH(const Instruction& i);
    void SW(const Instruction& i);

    void ADDI(const Instruction& i);
    void SLTI(const Instruction& i);
    void SLTIU(const Instruction& i);
    void XORI(const Instruction& i);
    void ORI(const Instruction& i);
    void ANDI(const Instruction& i);
    void SLLI(const Instruction& i);
    void SRLI(const Instruction& i);
    void SRAI(const Instruction& i);

    void ADD(const Instruction& i);
    void SUB(const Instruction& i);
    void SLL(const Instruction& i);
    void SLT(const Instruction& i);
    void SLTU(const Instruction& i);
    void XOR(const Instruction& i);
    void SRL(const Instruction& i);
    void SRA(const Instruction& i);
    void OR(const Instruction& i);
    void AND(const Instruction& i);

    void FENCE(const Instruction& i);
    bool ECALL(const Instruction& i);
};

// ─── Immediate-type instructions ──────────────────────────────────────

inline void Executor::LUI(const Instruction& i)
{
    regs->setValue(i.rd(), i.imm_U());
}

inline void Executor::AUIPC(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getPC() + i.imm_U());
}

inline bool Executor::JAL(const Instruction& i)
{
    uint32_t link = regs->getPC() + 4;
    regs->setValue(i.rd(), link);
    regs->setPC(regs->getPC() + i.imm_J());
    return true;
}

inline bool Executor::JALR(const Instruction& i)
{
    uint32_t link = regs->getPC() + 4;
    uint32_t target = (regs->getValue(i.rs1()) + i.imm_I()) & ~1u;
    regs->setValue(i.rd(), link);
    regs->setPC(target);
    return true;
}

// ─── Branch instructions ──────────────────────────────────────────────

inline bool Executor::BEQ(const Instruction& i)
{
    if (regs->getValue(i.rs1()) == regs->getValue(i.rs2()))
    {
        regs->setPC(regs->getPC() + i.imm_B());
        return true;
    }
    return false;
}
inline bool Executor::BNE(const Instruction& i)
{
    if (regs->getValue(i.rs1()) != regs->getValue(i.rs2()))
    {
        regs->setPC(regs->getPC() + i.imm_B());
        return true;
    }
    return false;
}
inline bool Executor::BLT(const Instruction& i)
{
    if ((int32_t)regs->getValue(i.rs1()) < (int32_t)regs->getValue(i.rs2()))
    {
        regs->setPC(regs->getPC() + i.imm_B());
        return true;
    }
    return false;
}
inline bool Executor::BGE(const Instruction& i)
{
    if ((int32_t)regs->getValue(i.rs1()) >= (int32_t)regs->getValue(i.rs2()))
    {
        regs->setPC(regs->getPC() + i.imm_B());
        return true;
    }
    return false;
}
inline bool Executor::BLTU(const Instruction& i)
{
    if (regs->getValue(i.rs1()) < regs->getValue(i.rs2()))
    {
        regs->setPC(regs->getPC() + i.imm_B());
        return true;
    }
    return false;
}
inline bool Executor::BGEU(const Instruction& i)
{
    if (regs->getValue(i.rs1()) >= regs->getValue(i.rs2()))
    {
        regs->setPC(regs->getPC() + i.imm_B());
        return true;
    }
    return false;
}

// ─── Load instructions ────────────────────────────────────────────────

inline void Executor::LB(const Instruction& i)
{
    uint64_t addr = regs->getValue(i.rs1()) + i.imm_I();
    int8_t val = (int8_t)mem_if->readDataMem(addr, 1);
    regs->setValue(i.rd(), (int32_t)val);
}
inline void Executor::LH(const Instruction& i)
{
    uint64_t addr = regs->getValue(i.rs1()) + i.imm_I();
    int16_t val = (int16_t)mem_if->readDataMem(addr, 2);
    regs->setValue(i.rd(), (int32_t)val);
}
inline void Executor::LW(const Instruction& i)
{
    uint64_t addr = regs->getValue(i.rs1()) + i.imm_I();
    uint32_t val = mem_if->readDataMem(addr, 4);
    regs->setValue(i.rd(), val);
}
inline void Executor::LBU(const Instruction& i)
{
    uint64_t addr = regs->getValue(i.rs1()) + i.imm_I();
    uint8_t val = (uint8_t)mem_if->readDataMem(addr, 1);
    regs->setValue(i.rd(), (uint32_t)val);
}
inline void Executor::LHU(const Instruction& i)
{
    uint64_t addr = regs->getValue(i.rs1()) + i.imm_I();
    uint16_t val = (uint16_t)mem_if->readDataMem(addr, 2);
    regs->setValue(i.rd(), (uint32_t)val);
}

// ─── Store instructions ───────────────────────────────────────────────

inline void Executor::SB(const Instruction& i)
{
    uint64_t addr = regs->getValue(i.rs1()) + i.imm_S();
    mem_if->writeDataMem(addr, regs->getValue(i.rs2()), 1);
}
inline void Executor::SH(const Instruction& i)
{
    uint64_t addr = regs->getValue(i.rs1()) + i.imm_S();
    mem_if->writeDataMem(addr, regs->getValue(i.rs2()), 2);
}
inline void Executor::SW(const Instruction& i)
{
    uint64_t addr = regs->getValue(i.rs1()) + i.imm_S();
    mem_if->writeDataMem(addr, regs->getValue(i.rs2()), 4);
}

// ─── ALU immediate ────────────────────────────────────────────────────

inline void Executor::ADDI(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) + i.imm_I());
}
inline void Executor::SLTI(const Instruction& i)
{
    regs->setValue(i.rd(), ((int32_t)regs->getValue(i.rs1()) < i.imm_I()) ? 1 : 0);
}
inline void Executor::SLTIU(const Instruction& i)
{
    regs->setValue(i.rd(), (regs->getValue(i.rs1()) < (uint32_t)i.imm_I()) ? 1 : 0);
}
inline void Executor::XORI(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) ^ i.imm_I());
}
inline void Executor::ORI(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) | i.imm_I());
}
inline void Executor::ANDI(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) & i.imm_I());
}
inline void Executor::SLLI(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) << i.shamt());
}
inline void Executor::SRLI(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) >> i.shamt());
}
inline void Executor::SRAI(const Instruction& i)
{
    regs->setValue(i.rd(), (int32_t)regs->getValue(i.rs1()) >> i.shamt());
}

// ─── ALU register-register ────────────────────────────────────────────

inline void Executor::ADD(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) + regs->getValue(i.rs2()));
}
inline void Executor::SUB(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) - regs->getValue(i.rs2()));
}
inline void Executor::SLL(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) << (regs->getValue(i.rs2()) & 0x1F));
}
inline void Executor::SLT(const Instruction& i)
{
    regs->setValue(i.rd(),
                   ((int32_t)regs->getValue(i.rs1()) < (int32_t)regs->getValue(i.rs2())) ? 1 : 0);
}
inline void Executor::SLTU(const Instruction& i)
{
    regs->setValue(i.rd(), (regs->getValue(i.rs1()) < regs->getValue(i.rs2())) ? 1 : 0);
}
inline void Executor::XOR(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) ^ regs->getValue(i.rs2()));
}
inline void Executor::SRL(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) >> (regs->getValue(i.rs2()) & 0x1F));
}
inline void Executor::SRA(const Instruction& i)
{
    regs->setValue(i.rd(), (int32_t)regs->getValue(i.rs1()) >> (regs->getValue(i.rs2()) & 0x1F));
}
inline void Executor::OR(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) | regs->getValue(i.rs2()));
}
inline void Executor::AND(const Instruction& i)
{
    regs->setValue(i.rd(), regs->getValue(i.rs1()) & regs->getValue(i.rs2()));
}

// ─── System ───────────────────────────────────────────────────────────

inline void Executor::FENCE(const Instruction& /*i*/)
{
    /* NOP */
}

inline bool Executor::ECALL(const Instruction& /*i*/)
{
    std::cout << "\n=== ECALL: simulation stopped ===" << std::endl;
    regs->dump();
    sc_core::sc_stop();
    return false;
}

// ─── Decode + Execute ─────────────────────────────────────────────────

inline bool Executor::execute(uint32_t instr_raw)
{
    Instruction inst(instr_raw);

    switch (inst.opcode())
    {
        case 0b0110111:
            LUI(inst);
            return false;
        case 0b0010111:
            AUIPC(inst);
            return false;
        case 0b1101111:
            return JAL(inst);
        case 0b1100111:
            return JALR(inst);

        case 0b1100011:
            switch (inst.funct3())
            {
                case 0b000:
                    return BEQ(inst);
                case 0b001:
                    return BNE(inst);
                case 0b100:
                    return BLT(inst);
                case 0b101:
                    return BGE(inst);
                case 0b110:
                    return BLTU(inst);
                case 0b111:
                    return BGEU(inst);
            }
            break;

        case 0b0000011:
            switch (inst.funct3())
            {
                case 0b000:
                    LB(inst);
                    return false;
                case 0b001:
                    LH(inst);
                    return false;
                case 0b010:
                    LW(inst);
                    return false;
                case 0b100:
                    LBU(inst);
                    return false;
                case 0b101:
                    LHU(inst);
                    return false;
            }
            break;

        case 0b0100011:
            switch (inst.funct3())
            {
                case 0b000:
                    SB(inst);
                    return false;
                case 0b001:
                    SH(inst);
                    return false;
                case 0b010:
                    SW(inst);
                    return false;
            }
            break;

        case 0b0010011:
            switch (inst.funct3())
            {
                case 0b000:
                    ADDI(inst);
                    return false;
                case 0b010:
                    SLTI(inst);
                    return false;
                case 0b011:
                    SLTIU(inst);
                    return false;
                case 0b100:
                    XORI(inst);
                    return false;
                case 0b110:
                    ORI(inst);
                    return false;
                case 0b111:
                    ANDI(inst);
                    return false;
                case 0b001:
                    SLLI(inst);
                    return false;
                case 0b101:
                    switch (inst.funct7())
                    {
                        case 0b0000000:
                            SRLI(inst);
                            return false;
                        case 0b0100000:
                            SRAI(inst);
                            return false;
                    }
                    break;
            }
            break;

        case 0b0110011:
            switch (inst.funct3())
            {
                case 0b000:
                    switch (inst.funct7())
                    {
                        case 0b0000000:
                            ADD(inst);
                            return false;
                        case 0b0100000:
                            SUB(inst);
                            return false;
                    }
                    break;
                case 0b001:
                    SLL(inst);
                    return false;
                case 0b010:
                    SLT(inst);
                    return false;
                case 0b011:
                    SLTU(inst);
                    return false;
                case 0b100:
                    XOR(inst);
                    return false;
                case 0b101:
                    switch (inst.funct7())
                    {
                        case 0b0000000:
                            SRL(inst);
                            return false;
                        case 0b0100000:
                            SRA(inst);
                            return false;
                    }
                    break;
                case 0b110:
                    OR(inst);
                    return false;
                case 0b111:
                    AND(inst);
                    return false;
            }
            break;

        case 0b0001111:
            FENCE(inst);
            return false;

        case 0b1110011:
            if (inst.funct3() == 0b000)
            {
                if (inst.imm_I() == 0)
                    return ECALL(inst);
            }
            break;
    }

    std::cerr << "Illegal instruction: 0x" << std::hex << instr_raw << " @ PC=0x" << regs->getPC()
              << std::dec << std::endl;
    return false;
}

}  // namespace riscv_soc_tlm

#endif
