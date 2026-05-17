#include <iostream>
#include <string>
#include "cpu/rv32-lt/cpu.h"
#include "mem/memory.h"

using namespace rv32;

int sc_main(int argc, char* argv[])
{
    std::string hexfile = "firmware.hex";
    if (argc > 1)
    {
        hexfile = argv[1];
    }

    std::cout << "Loading " << hexfile << std::endl;

    Memory memory("memory");
    memory.loadHex(hexfile);

    CPU cpu("cpu", memory.getStartPC());

    cpu.instr_socket.bind(memory.socket);
    cpu.mem_if.data_socket.bind(memory.socket);

    std::cout << "Starting simulation..." << std::endl;
    sc_core::sc_start();

    std::cout << "Simulation finished." << std::endl;
    return 0;
}
