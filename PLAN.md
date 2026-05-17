# my_rv32 — 最简 RV32I TLM 模拟器实施方案

## 目标

实现一个可运行 RV32I 基础整数指令集的 SystemC TLM-2.0 模拟器，
能加载 Intel HEX 文件，执行到 ECALL 后停止并打印寄存器状态。

不支持：中断、CSR、特权模式、C/M/A/F/D 扩展、GDB、外设。

---

## 文件清单（7个源文件 + 1个 Makefile）

```
my_rv32/
├── Makefile
├── main.cpp            # 拓扑组装 + sc_main
├── CPU.h               # CPU 类声明
├── CPU.cpp             # CPU_thread + CPU_step (取指/解码/执行/PC更新)
├── Registers.h         # 寄存器文件 (x0-x31 + PC)
├── Memory.h            # Memory 类声明
├── Memory.cpp          # TLM target + HEX 加载
└── BASE_ISA.h          # RV32I 指令集解码 + 执行
```

---

## 前置依赖

- SystemC 2.3.3+（C++17 编译）
- spdlog（可选，用 `std::cout` 替代也行）
- g++ / clang++

SystemC 环境变量（安装后设置）：

```sh
export SYSTEMC_HOME=/path/to/systemc
export SPDLOG_HOME=/path/to/spdlog   # 如果不用 spdlog 可以省略
```

---

## 各模块规格

### 1. Registers.h

```
功能：寄存 32 个通用寄存器和 PC

数据结构：
  std::array<uint32_t, 32> regs;   // regs[0] 始终为 0
  uint32_t pc;                      // 默认值 0x80000000

对外接口：
  uint32_t getValue(uint32_t reg_num) const;   // reg_num 范围 0-31
  void     setValue(uint32_t reg_num, uint32_t value);  // x0 不可写
  uint32_t getPC() const;
  void     setPC(uint32_t new_pc);
  void     incPC();                             // pc += 4
  void     dump() const;                        // 打印所有寄存器（ECALL 时用）

约束：
  - getValue(0) 始终返回 0
  - setValue(0, val) 什么都不做
```

### 2. Memory.h / Memory.cpp

```
功能：TLM-2.0 target 模块，响应读写请求 + 加载 Intel HEX

继承：sc_core::sc_module

TLM Socket：
  tlm_utils::simple_target_socket<Memory> socket;

构造函数：
  Memory(sc_module_name name)
    - 注册 b_transport 回调
    - 调用 loadHex("firmware.hex") 加载程序
    - 文件名可以从命令行参数获取

b_transport 实现：
  - 收到 TLM_READ_COMMAND: 从内部 mem[] 拷贝数据到 txn.get_data_ptr()
  - 收到 TLM_WRITE_COMMAND: 从 txn.get_data_ptr() 拷贝数据到内部 mem[]
  - 支持 1/2/4 字节宽度（通过 txn.get_data_length() 判断）
  - 设置 response_status = TLM_OK_RESPONSE

Intel HEX 解析（loadHex）：
  - 只处理记录类型 00（数据）和 01（EOF）
  - 类型 00: ":" LL AAAA 00 DDDD... CC → 写入 mem[AAAA...]
  - 类型 01: 结束，打印 "HEX loaded"
  - 返回程序入口地址（首条 00 记录的地址），供 CPU 设置初始 PC

内部存储：
  static constexpr uint32_t SIZE = 8 * 1024 * 1024;  // 8MB
  uint8_t mem[SIZE];

约束：
  - 不需要 DMI
  - 不需要时序建模（delay 传 0 就够）
  - 地址范围检查：超出 SIZE 时报错
```

### 3. BASE_ISA.h

```
功能：RV32I 指令解码和执行（纯头文件，无模板）

命名空间：rv32

核心数据结构 — Instruction 类（或直接放在 BASE_ISA 内部）：

  class Instruction {
      uint32_t m_instr;
  public:
      Instruction(uint32_t instr) : m_instr(instr) {}

      // 字段提取（用 inline 位操作实现）
      uint32_t opcode()    const { return m_instr & 0x7F; }
      uint32_t rd()        const { return (m_instr >> 7)  & 0x1F; }
      uint32_t funct3()    const { return (m_instr >> 12) & 0x7; }
      uint32_t rs1()       const { return (m_instr >> 15) & 0x1F; }
      uint32_t rs2()       const { return (m_instr >> 20) & 0x1F; }
      uint32_t funct7()    const { return (m_instr >> 25) & 0x7F; }
      uint32_t getInstr()  const { return m_instr; }

      // 立即数提取（需要符号扩展）
      int32_t  imm_I() const;   // instr[31:20]
      int32_t  imm_S() const;   // instr[31:25] << 5 | instr[11:7]
      int32_t  imm_B() const;   // instr[31|7|30:25|11:8] << 1
      uint32_t imm_U() const;   // instr[31:12] << 12
      int32_t  imm_J() const;   // instr[31|19:12|20|30:21] << 1
  };

执行类 — Executor：

  class Executor {
      Registers* regs;
      Memory*    mem;   // 直接引用 Memory 来 readData/writeData
  public:
      Executor(Registers* r, Memory* m) : regs(r), mem(m) {}

      // 执行入口：根据 opcode 分发
      // 返回值: true = PC 已被指令修改（跳转/分支）, false = PC 正常 +4
      bool execute(uint32_t instr_raw);

      // 每个指令一个执行函数
      void exec_LUI(const Instruction& inst);
      void exec_AUIPC(const Instruction& inst);
      bool exec_JAL(const Instruction& inst);
      bool exec_JALR(const Instruction& inst);
      bool exec_BEQ(const Instruction& inst);
      bool exec_BNE(const Instruction& inst);
      bool exec_BLT(const Instruction& inst);
      bool exec_BGE(const Instruction& inst);
      bool exec_BLTU(const Instruction& inst);
      bool exec_BGEU(const Instruction& inst);
      void exec_LB(const Instruction& inst);
      void exec_LH(const Instruction& inst);
      void exec_LW(const Instruction& inst);
      void exec_LBU(const Instruction& inst);
      void exec_LHU(const Instruction& inst);
      void exec_SB(const Instruction& inst);
      void exec_SH(const Instruction& inst);
      void exec_SW(const Instruction& inst);
      void exec_ADDI(const Instruction& inst);
      void exec_SLTI(const Instruction& inst);
      void exec_SLTIU(const Instruction& inst);
      void exec_XORI(const Instruction& inst);
      void exec_ORI(const Instruction& inst);
      void exec_ANDI(const Instruction& inst);
      void exec_SLLI(const Instruction& inst);
      void exec_SRLI(const Instruction& inst);
      void exec_SRAI(const Instruction& inst);
      void exec_ADD(const Instruction& inst);
      void exec_SUB(const Instruction& inst);
      void exec_SLL(const Instruction& inst);
      void exec_SLT(const Instruction& inst);
      void exec_SLTU(const Instruction& inst);
      void exec_XOR(const Instruction& inst);
      void exec_SRL(const Instruction& inst);
      void exec_SRA(const Instruction& inst);
      void exec_OR(const Instruction& inst);
      void exec_AND(const Instruction& inst);
      void exec_FENCE(const Instruction& inst);   // NOP
      bool exec_ECALL(const Instruction& inst);   // 打印 regs，返回 true 表示停止
  };

execute() 的完整解码逻辑（按 opcode 值）：

  opcode = instr_raw & 0x7F;
  switch (opcode) {
      case 0b0110111: exec_LUI(inst);     return false;
      case 0b0010111: exec_AUIPC(inst);   return false;
      case 0b1101111: return exec_JAL(inst);
      case 0b1100111: return exec_JALR(inst);

      case 0b1100011:
          switch (inst.funct3()) {
              case 0b000: return exec_BEQ(inst);
              case 0b001: return exec_BNE(inst);
              case 0b100: return exec_BLT(inst);
              case 0b101: return exec_BGE(inst);
              case 0b110: return exec_BLTU(inst);
              case 0b111: return exec_BGEU(inst);
          }

      case 0b0000011:
          switch (inst.funct3()) {
              case 0b000: exec_LB(inst);   return false;
              case 0b001: exec_LH(inst);   return false;
              case 0b010: exec_LW(inst);   return false;
              case 0b100: exec_LBU(inst);  return false;
              case 0b101: exec_LHU(inst);  return false;
          }

      case 0b0100011:
          switch (inst.funct3()) {
              case 0b000: exec_SB(inst);   return false;
              case 0b001: exec_SH(inst);   return false;
              case 0b010: exec_SW(inst);   return false;
          }

      case 0b0010011:
          switch (inst.funct3()) {
              case 0b000: exec_ADDI(inst);  return false;
              case 0b010: exec_SLTI(inst);  return false;
              case 0b011: exec_SLTIU(inst); return false;
              case 0b100: exec_XORI(inst);  return false;
              case 0b110: exec_ORI(inst);   return false;
              case 0b111: exec_ANDI(inst);  return false;
              case 0b001: exec_SLLI(inst);  return false;
              case 0b101:
                  switch (inst.funct7()) {
                      case 0b0000000: exec_SRLI(inst); return false;
                      case 0b0100000: exec_SRAI(inst); return false;
                  }
          }

      case 0b0110011:
          switch (inst.funct3()) {
              case 0b000:
                  switch (inst.funct7()) {
                      case 0b0000000: exec_ADD(inst); return false;
                      case 0b0100000: exec_SUB(inst); return false;
                  }
              case 0b001: exec_SLL(inst);  return false;
              case 0b010: exec_SLT(inst);  return false;
              case 0b011: exec_SLTU(inst); return false;
              case 0b100: exec_XOR(inst);  return false;
              case 0b101:
                  switch (inst.funct7()) {
                      case 0b0000000: exec_SRL(inst); return false;
                      case 0b0100000: exec_SRA(inst); return false;
                  }
              case 0b110: exec_OR(inst);   return false;
              case 0b111: exec_AND(inst);  return false;
          }

      case 0b0001111: exec_FENCE(inst); return false;   // NOP

      case 0b1110011:
          switch (inst.funct3()) {
              case 0b000:
                  if (inst.imm_I() == 0) return exec_ECALL(inst);  // ECALL
                  if (inst.imm_I() == 1) { /* EBREAK */ }
          }

      default: std::cerr << "Unknown opcode" << std::endl; break;
  }
  return false;

指令执行函数编写注意事项：

  - LUI:   rd = imm_U
  - AUIPC: rd = pc + imm_U
  - JAL:   rd = pc+4; pc = pc+imm_J
  - JALR:  rd = pc+4; pc = (rs1 + imm_I) & ~1
  - 分支：用 signed_T 比较（通过 (int32_t) 强制转换）
  - LOAD: 从 Memory 读数据（需要 Memory 提供 readMem(addr, size) 方法）
  - STORE: 向 Memory 写数据（需要 Memory 提供 writeMem(addr, data, size) 方法）
  - 移位：shamt = 低5位
  - ECALL: 打印 "ECALL" + dump 寄存器，然后 sc_stop()
  - FENCE: 什么都不做

提示：
  直接复用 RISC-V-TLM 中 BASE_ISA.h 的 Exec_XXX() 函数体逻辑，
  每个函数只有 5-10 行，核心就是：
    1. 从 inst 取 rd/rs1/rs2
    2. 从 regs 取值
    3. 计算
    4. 写回 regs 或 mem
```

### 4. CPU.h / CPU.cpp

```
功能：CPU 顶层模块，驱动取指-解码-执行循环

继承：sc_core::sc_module

关键成员：
  Registers          regs;            // 寄存器文件
  Executor           executor;        // 执行单元
  Memory&            memory;          // 引用 memory 实例
  sc_core::sc_event  step_event;      // 用于 GDB 模式（可选）

TLM Socket：
  tlm_utils::simple_initiator_socket<CPU> instr_socket;   // 指令总线
  tlm_utils::simple_initiator_socket<CPU> data_socket;     // 数据总线
  （或者都连到同一个 socket，看 Memory 怎么设计）

构造参数：
  CPU(sc_module_name name, Memory& mem, uint32_t start_pc)

SC_THREAD: CPU_thread()

CPU_thread() 主循环伪代码：

  void CPU_thread() {
      while (true) {
          // 1. Fetch
          uint32_t instr_raw;
          fetchInstruction(instr_raw);   // 通过 instr_socket b_transport 读 4 字节

          // 2. Decode + Execute
          bool pc_updated = executor.execute(instr_raw);

          // 3. Update PC
          if (!pc_updated) {
              regs.incPC();
          }

          // 4. Wait for next cycle
          sc_core::wait(10, SC_NS);    // 固定 100MHz
      }
  }

fetchInstruction() 实现细节：

  tlm::tlm_generic_payload trans;
  sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
  uint32_t data;

  trans.set_command(tlm::TLM_READ_COMMAND);
  trans.set_address(regs.getPC());
  trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
  trans.set_data_length(4);
  trans.set_streaming_width(4);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  instr_socket->b_transport(trans, delay);

  if (trans.is_response_error()) {
      SC_REPORT_ERROR("CPU", "Instruction fetch failed");
  }

  instr_raw = data;

注意：instr_socket 和 data_socket 都连到同一个 Memory target socket。
Memory 不区分指令和数据访问，都从同一块内存读写。
```

### 5. main.cpp

```
功能：组装仿真拓扑 + 命令解析

int sc_main(int argc, char* argv[]) {
    // 1. 解析命令行参数
    std::string hexfile = "firmware.hex";
    if (argc > 1) {
        hexfile = argv[1];
    }

    // 2. 创建模块
    Memory memory("memory");
    memory.loadHex(hexfile);              // 返回 start_pc

    CPU cpu("cpu", memory, memory.getStartPC());

    // 3. 绑定 TLM socket
    cpu.instr_socket.bind(memory.socket);
    cpu.data_socket.bind(memory.socket);

    // 4. 启动仿真
    sc_core::sc_start();

    // 5. 打印结果
    cpu.regs.dump();

    return 0;
}
```

### 6. Makefile

```makefile
# Compiler settings
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2 -g
LDFLAGS  =

# SystemC paths
SYSTEMC_HOME  = $(HOME)/opt/systemc-2.3.4
SYSTEMC_INC   = $(SYSTEMC_HOME)/include
SYSTEMC_LIB   = $(SYSTEMC_HOME)/lib

# Include paths
INCLUDES = -I$(SYSTEMC_INC) -I.

# Libraries
LIBS = -L$(SYSTEMC_LIB) -lsystemc -lpthread

# Source files
SRCS = main.cpp Memory.cpp CPU.cpp
OBJS = $(SRCS:.cpp=.o)

# Target
TARGET = my_rv32

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET) firmware.hex
```

---

## 开发执行顺序（推荐）

```
Phase 1 ─ 基础骨架
  ① 安装/检查 SystemC 环境，写一个能编译通过的 hello_sc.cpp
  ② 写 Makefile，确保 SystemC 链接正确
  ③ 写 Memory.h + Memory.cpp — 先只做 HEX 加载，打印加载结果
  ④ 写 main.cpp — 创建 Memory 实例，调用 loadHex，sc_start() 空转

Phase 2 ─ 寄存器 + 指令
  ⑤ 写 Registers.h — 最基本的 get/set/dump
  ⑥ 写 BASE_ISA.h 的 Instruction 类 — 字段提取函数
  ⑦ 写 BASE_ISA.h 的 Executor::execute() — 先只支持 ADDI, ADD

Phase 3 ─ CPU + Fetch
  ⑧ 写 CPU.h + CPU.cpp — fetch + 调用 executor
  ⑨ 在 main.cpp 中连接 CPU 和 Memory socket

Phase 4 ─ 逐步加指令
  ⑩ 加 LUI, AUIPC, JAL, JALR → 能跑跳转
  ⑪ 加 LW, SW → 能跑内存读写
  ⑫ 加分支 BEQ~BGEU → 能跑条件和循环
  ⑬ 加其余运算 → OR, AND, XOR, 移位等
  ⑭ 加 ECALL → 能正常停止

Phase 5 ─ 验证
  ⑮ 写一个简单汇编测试程序（见下方测试用例）
  ⑯ 用 riscv-gnu-toolchain 编译成 HEX
  ⑰ 运行，检查结果
```

---

## 测试用例

### 测试 1：基本算术

```asm
# test_alu.S
    .section .text
    .globl _start
_start:
    addi x1, x0, 10        # x1 = 10
    addi x2, x0, 20        # x2 = 20
    add  x3, x1, x2        # x3 = 30
    sub  x4, x2, x1        # x4 = 10
    ori  x5, x0, 0xFF      # x5 = 255
    andi x6, x5, 0x0F      # x6 = 15
    ecall

# 预期结果: x1=10, x2=20, x3=30, x4=10, x5=255, x6=15
```

### 测试 2：内存读写

```asm
# test_mem.S
    .section .text
    .globl _start
_start:
    addi x1, x0, 0x42      # x1 = 0x42
    addi x2, x0, 0x100     # x2 = 基地址 0x100
    sw   x1, 0(x2)         # mem[0x100] = 0x42
    lw   x3, 0(x2)         # x3 = mem[0x100]
    ecall

# 预期结果: x1=0x42, x3=0x42
```

### 测试 3：循环

```asm
# test_loop.S
    .section .text
    .globl _start
_start:
    addi x1, x0, 5         # x1 = loop counter
    addi x2, x0, 0         # x2 = accumulator
loop:
    add  x2, x2, x1        # x2 += x1
    addi x1, x1, -1        # x1--
    bne  x1, x0, loop      # if x1 != 0, goto loop
    ecall

# 预期结果: x1=0, x2=15 (5+4+3+2+1)
```

### 测试 4：函数调用

```asm
# test_call.S
    .section .text
    .globl _start
_start:
    addi x1, x0, 3
    jal  x3, double_it     # ra=x3, call double_it
    ecall

double_it:
    add  x1, x1, x1        # x1 = x1 * 2
    jalr x0, x3, 0         # return
```

---

## 常见坑点

1. **x0 硬连线为 0**：setValue(0, val) 必须什么都不做
2. **符号扩展**：imm_I/imm_S/imm_B/imm_J 返回的是 `int32_t`，值可能是负数
3. **分支偏移单位是字节**：imm_B 直接加到 PC，不需要乘以 2
4. **JALR 清 LSB**：`(rs1 + imm_I) & ~1`，确保目标地址对齐
5. **byte/half 加载**：LB/LBU/LH/LHU 需要符号扩展，用 `int8_t/int16_t` 中转
6. **SystemC 链接**：你的 SystemC 在 `~/opt/systemc-2.3.4`，Makefile 已配好；如果是 Apple Silicon Mac，需确认 SystemC 也是 arm64 编译的
7. **HEX 地址**：Intel HEX 中的地址是字节地址，直接用作内存偏移
