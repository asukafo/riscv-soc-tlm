#include <iostream>
#include <string>

#include "cpu/rv32i/cpu.h"
#include "interconnect/interconnect.h"
#include "mem/memory.h"

using namespace riscv_soc_tlm;

int sc_main(int argc, char* argv[])
{
    std::string hexfile = "firmware.hex";
    if (argc > 1)
    {
        hexfile = argv[1];
    }

    std::cout << "Loading " << hexfile << std::endl;

    Memory memory("memory");
    uint32_t start_pc = memory.loadHex(hexfile);

    Interconnect interconnect("interconnect");

    CPU cpu("cpu", start_pc);

    cpu.mem_if.socket.bind(interconnect.target_socket);

    interconnect.map(0x00000000, Memory::SIZE, interconnect.mem_socket);
    interconnect.map(0x80000000, Memory::SIZE, interconnect.mem_socket);

    interconnect.mem_socket.bind(memory.socket);

    std::cout << "Starting simulation..." << std::endl;
    sc_core::sc_start();

    std::cout << "Simulation finished." << std::endl;
    return 0;
}
