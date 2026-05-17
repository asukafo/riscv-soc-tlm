#ifndef __BASE_ISA_H__
#define __BASE_ISA_H__

#include <cstdint>
#include <iostream>
#include "systemc"

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
    using MemReadFn = uint32_t (*)(void*, uint64_t, int);
    using MemWriteFn = void (*)(void*, uint64_t, uint32_t, int);
    using RegReadFn = uint32_t (*)(void*, uint32_t);
    using RegWriteFn = void (*)(void*, uint32_t, uint32_t);
    using GetPCFn = uint32_t (*)(void*);
    using SetPCFn = void (*)(void*, uint32_t);
    using IncPCFn = void (*)(void*);
    using DumpFn = void (*)(void*);

    Executor()
        : mem_read(nullptr), mem_write(nullptr), reg_read(nullptr), reg_write(nullptr),
          get_pc(nullptr), set_pc(nullptr), inc_pc(nullptr), dump(nullptr), ctx(nullptr)
    {
    }

    void setContext(void* c) { ctx = c; }
    void setMem(MemReadFn r, MemWriteFn w) { mem_read = r; mem_write = w; }
    void setReg(RegReadFn r, RegWriteFn w) { reg_read = r; reg_write = w; }
    void setPC(GetPCFn g, SetPCFn s, IncPCFn i) { get_pc = g; set_pc = s; inc_pc = i; }
    void setDump(DumpFn d) { dump = d; }

    bool execute(uint32_t instr_raw);

private:
    MemReadFn mem_read;
    MemWriteFn mem_write;
    RegReadFn reg_read;
    RegWriteFn reg_write;
    GetPCFn get_pc;
    SetPCFn set_pc;
    IncPCFn inc_pc;
    DumpFn dump;
    void* ctx;

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
    reg_write(ctx, i.rd(), i.imm_U());
}

inline void Executor::AUIPC(const Instruction& i)
{
    reg_write(ctx, i.rd(), get_pc(ctx) + i.imm_U());
}

inline bool Executor::JAL(const Instruction& i)
{
    uint32_t link = get_pc(ctx) + 4;
    reg_write(ctx, i.rd(), link);
    set_pc(ctx, get_pc(ctx) + i.imm_J());
    return true;
}

inline bool Executor::JALR(const Instruction& i)
{
    uint32_t link = get_pc(ctx) + 4;
    uint32_t target = (reg_read(ctx, i.rs1()) + i.imm_I()) & ~1u;
    reg_write(ctx, i.rd(), link);
    set_pc(ctx, target);
    return true;
}

// ─── Branch instructions ──────────────────────────────────────────────

inline bool Executor::BEQ(const Instruction& i)
{
    if (reg_read(ctx, i.rs1()) == reg_read(ctx, i.rs2()))
    {
        set_pc(ctx, get_pc(ctx) + i.imm_B());
        return true;
    }
    return false;
}
inline bool Executor::BNE(const Instruction& i)
{
    if (reg_read(ctx, i.rs1()) != reg_read(ctx, i.rs2()))
    {
        set_pc(ctx, get_pc(ctx) + i.imm_B());
        return true;
    }
    return false;
}
inline bool Executor::BLT(const Instruction& i)
{
    if ((int32_t)reg_read(ctx, i.rs1()) < (int32_t)reg_read(ctx, i.rs2()))
    {
        set_pc(ctx, get_pc(ctx) + i.imm_B());
        return true;
    }
    return false;
}
inline bool Executor::BGE(const Instruction& i)
{
    if ((int32_t)reg_read(ctx, i.rs1()) >= (int32_t)reg_read(ctx, i.rs2()))
    {
        set_pc(ctx, get_pc(ctx) + i.imm_B());
        return true;
    }
    return false;
}
inline bool Executor::BLTU(const Instruction& i)
{
    if (reg_read(ctx, i.rs1()) < reg_read(ctx, i.rs2()))
    {
        set_pc(ctx, get_pc(ctx) + i.imm_B());
        return true;
    }
    return false;
}
inline bool Executor::BGEU(const Instruction& i)
{
    if (reg_read(ctx, i.rs1()) >= reg_read(ctx, i.rs2()))
    {
        set_pc(ctx, get_pc(ctx) + i.imm_B());
        return true;
    }
    return false;
}

// ─── Load instructions ────────────────────────────────────────────────

inline void Executor::LB(const Instruction& i)
{
    uint64_t addr = reg_read(ctx, i.rs1()) + i.imm_I();
    int8_t val = (int8_t)mem_read(ctx, addr, 1);
    reg_write(ctx, i.rd(), (int32_t)val);
}
inline void Executor::LH(const Instruction& i)
{
    uint64_t addr = reg_read(ctx, i.rs1()) + i.imm_I();
    int16_t val = (int16_t)mem_read(ctx, addr, 2);
    reg_write(ctx, i.rd(), (int32_t)val);
}
inline void Executor::LW(const Instruction& i)
{
    uint64_t addr = reg_read(ctx, i.rs1()) + i.imm_I();
    uint32_t val = mem_read(ctx, addr, 4);
    reg_write(ctx, i.rd(), val);
}
inline void Executor::LBU(const Instruction& i)
{
    uint64_t addr = reg_read(ctx, i.rs1()) + i.imm_I();
    uint8_t val = (uint8_t)mem_read(ctx, addr, 1);
    reg_write(ctx, i.rd(), (uint32_t)val);
}
inline void Executor::LHU(const Instruction& i)
{
    uint64_t addr = reg_read(ctx, i.rs1()) + i.imm_I();
    uint16_t val = (uint16_t)mem_read(ctx, addr, 2);
    reg_write(ctx, i.rd(), (uint32_t)val);
}

// ─── Store instructions ───────────────────────────────────────────────

inline void Executor::SB(const Instruction& i)
{
    uint64_t addr = reg_read(ctx, i.rs1()) + i.imm_S();
    mem_write(ctx, addr, reg_read(ctx, i.rs2()), 1);
}
inline void Executor::SH(const Instruction& i)
{
    uint64_t addr = reg_read(ctx, i.rs1()) + i.imm_S();
    mem_write(ctx, addr, reg_read(ctx, i.rs2()), 2);
}
inline void Executor::SW(const Instruction& i)
{
    uint64_t addr = reg_read(ctx, i.rs1()) + i.imm_S();
    mem_write(ctx, addr, reg_read(ctx, i.rs2()), 4);
}

// ─── ALU immediate ────────────────────────────────────────────────────

inline void Executor::ADDI(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) + i.imm_I());
}
inline void Executor::SLTI(const Instruction& i)
{
    reg_write(ctx, i.rd(), ((int32_t)reg_read(ctx, i.rs1()) < i.imm_I()) ? 1 : 0);
}
inline void Executor::SLTIU(const Instruction& i)
{
    reg_write(ctx, i.rd(), (reg_read(ctx, i.rs1()) < (uint32_t)i.imm_I()) ? 1 : 0);
}
inline void Executor::XORI(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) ^ i.imm_I());
}
inline void Executor::ORI(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) | i.imm_I());
}
inline void Executor::ANDI(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) & i.imm_I());
}
inline void Executor::SLLI(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) << i.shamt());
}
inline void Executor::SRLI(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) >> i.shamt());
}
inline void Executor::SRAI(const Instruction& i)
{
    reg_write(ctx, i.rd(), (int32_t)reg_read(ctx, i.rs1()) >> i.shamt());
}

// ─── ALU register-register ────────────────────────────────────────────

inline void Executor::ADD(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) + reg_read(ctx, i.rs2()));
}
inline void Executor::SUB(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) - reg_read(ctx, i.rs2()));
}
inline void Executor::SLL(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) << (reg_read(ctx, i.rs2()) & 0x1F));
}
inline void Executor::SLT(const Instruction& i)
{
    reg_write(ctx, i.rd(),
                   ((int32_t)reg_read(ctx, i.rs1()) < (int32_t)reg_read(ctx, i.rs2())) ? 1 : 0);
}
inline void Executor::SLTU(const Instruction& i)
{
    reg_write(ctx, i.rd(), (reg_read(ctx, i.rs1()) < reg_read(ctx, i.rs2())) ? 1 : 0);
}
inline void Executor::XOR(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) ^ reg_read(ctx, i.rs2()));
}
inline void Executor::SRL(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) >> (reg_read(ctx, i.rs2()) & 0x1F));
}
inline void Executor::SRA(const Instruction& i)
{
    reg_write(ctx, i.rd(), (int32_t)reg_read(ctx, i.rs1()) >> (reg_read(ctx, i.rs2()) & 0x1F));
}
inline void Executor::OR(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) | reg_read(ctx, i.rs2()));
}
inline void Executor::AND(const Instruction& i)
{
    reg_write(ctx, i.rd(), reg_read(ctx, i.rs1()) & reg_read(ctx, i.rs2()));
}

// ─── System ───────────────────────────────────────────────────────────

inline void Executor::FENCE(const Instruction& /*i*/)
{
    /* NOP */
}

inline bool Executor::ECALL(const Instruction& /*i*/)
{
    std::cout << "\n=== ECALL: simulation stopped ===" << std::endl;
    dump(ctx);
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

    std::cerr << "Illegal instruction: 0x" << std::hex << instr_raw << " @ PC=0x" << get_pc(ctx)
              << std::dec << std::endl;
    return false;
}

}  // namespace riscv_soc_tlm

#endif
