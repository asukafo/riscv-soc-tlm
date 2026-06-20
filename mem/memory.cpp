#include "mem/memory.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace riscv_soc_tlm
{

SC_HAS_PROCESS(Memory);

static uint32_t read32LE(const unsigned char* buf)
{
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

Memory::Memory(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      socket("socket"),
      read_latency(sc_core::sc_time(10, sc_core::SC_NS)),
      write_latency(sc_core::sc_time(10, sc_core::SC_NS)),
      tREFI(sc_core::sc_time(7800, sc_core::SC_NS)),   // 7.8 µs
      tRFC(sc_core::sc_time(70, sc_core::SC_NS)),       // 70 ns
      peq("peq", this, &Memory::peq_cb),
      base_addr(0x80000000)
{
    // LT path
    socket.register_b_transport(this, &Memory::b_transport);
    // AT path — Clause 15
    socket.register_nb_transport_fw(this, &Memory::nb_transport_fw);
    // DMI — Clause 11.3
    socket.register_get_direct_mem_ptr(this, &Memory::get_direct_mem_ptr);
    // Debug — Clause 11.4
    socket.register_transport_dbg(this, &Memory::transport_dbg);

    // P3.15: DRAM refresh thread
    SC_THREAD(refresh_thread);

    std::memset(mem, 0, SIZE);
}

// ═══════════════════════════════════════════════════════════════════════
// P3.15: DRAM Refresh — periodic refresh with tREFI=7.8µs, tRFC=70ns
//
// During refresh, new AT transactions are delayed (via nb_transport_fw
// adding the remaining tRFC to the PEQ delay) and LT transactions add
// the penalty to their delay parameter.
// ═══════════════════════════════════════════════════════════════════════
sc_core::sc_time Memory::refresh_penalty() const
{
    if (!m_in_refresh) return sc_core::SC_ZERO_TIME;

    sc_core::sc_time elapsed = sc_core::sc_time_stamp() - m_refresh_start;
    if (elapsed >= tRFC) return sc_core::SC_ZERO_TIME;

    return tRFC - elapsed;
}

void Memory::refresh_thread()
{
    while (true)
    {
        sc_core::wait(tREFI);  // wait for next refresh interval

        m_in_refresh = true;
        m_refresh_start = sc_core::sc_time_stamp();

        sc_core::wait(tRFC);   // refresh cycle

        m_in_refresh = false;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Clause 14.9/14.10: Access with streaming_width and byte_enable
// ═══════════════════════════════════════════════════════════════════════
void Memory::accessMem(uint64_t addr, unsigned char* ptr, uint32_t len,
                       tlm::tlm_command cmd, uint32_t streaming_width,
                       unsigned char* be_ptr, uint32_t be_len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        // streaming_width determines address stepping (identity when == len)
        uint64_t a = addr + (i % streaming_width);
        uint32_t offset = toOffset(a);
        if (offset >= SIZE) continue;

        // byte_enable — if present, skip disabled byte lanes
        bool en = !be_ptr || (be_ptr[i % be_len] == TLM_BYTE_ENABLED);
        if (!en) continue;

        if (cmd == tlm::TLM_READ_COMMAND)
            ptr[i] = mem[offset];
        else  // TLM_WRITE_COMMAND
            mem[offset] = ptr[i];
    }
}

// ═══════════════════════════════════════════════════════════════════════
// LT path — blocking b_transport
// ═══════════════════════════════════════════════════════════════════════
void Memory::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();
    uint32_t offset = toOffset(addr);
    unsigned char* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();
    uint32_t sw = trans.get_streaming_width();
    unsigned char* be = trans.get_byte_enable_ptr();
    uint32_t be_len = trans.get_byte_enable_length();

    if (offset + len > SIZE)
    {
        std::cerr << "Memory access out of bounds: addr=0x" << std::hex << addr
                  << " offset=0x" << offset << " len=" << std::dec << len << std::endl;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    accessMem(addr, ptr, len, trans.get_command(), sw, be, be_len);

    delay += (trans.get_command() == tlm::TLM_READ_COMMAND)
                 ? read_latency : write_latency;
    delay += refresh_penalty();  // P3.15: DRAM refresh stall

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    trans.set_dmi_allowed(true);  // Clause 11.3: hint that DMI is available
}

// ═══════════════════════════════════════════════════════════════════════
// AT path — non-blocking nb_transport_fw — Clause 15.2
//
// Called by upstream initiator. Acquires the trans and schedules
// processing through PEQ.  Never calls wait() directly.
// ═══════════════════════════════════════════════════════════════════════
tlm::tlm_sync_enum Memory::nb_transport_fw(tlm::tlm_generic_payload& trans,
                                            tlm::tlm_phase& phase,
                                            sc_core::sc_time& delay)
{
    // Clause 15.2.2: validate phase legality on forward path
    assert(phase == tlm::BEGIN_REQ || phase == tlm::END_RESP);

    sc_core::sc_time t = sc_core::SC_ZERO_TIME;
    if (phase == tlm::BEGIN_REQ)
    {
        // Clause 14.6: acquire refcount when taking ownership (once per trans)
        trans.acquire();

        t = (trans.get_command() == tlm::TLM_READ_COMMAND)
                ? read_latency : write_latency;
        t += refresh_penalty();  // P3.15: DRAM refresh stall
    }

    peq.notify(trans, phase, t);  // Clause 16.3: PEQ schedules the callback
    return tlm::TLM_ACCEPTED;
}

// ═══════════════════════════════════════════════════════════════════════
// PEQ callback — SC_METHOD, cannot call wait() — Clause 16.3
//
// Processes one (trans, phase) pair at a time.  For multi-phase
// transactions, re-schedules itself via peq.notify().
// ═══════════════════════════════════════════════════════════════════════
void Memory::peq_cb(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase)
{
    if (phase == tlm::BEGIN_REQ)
    {
        // ── Process the request ──────────────────────────────────
        uint64_t addr = trans.get_address();
        unsigned char* ptr = trans.get_data_ptr();
        uint32_t len = trans.get_data_length();
        uint32_t sw = trans.get_streaming_width();
        unsigned char* be = trans.get_byte_enable_ptr();
        uint32_t be_len = trans.get_byte_enable_length();

        accessMem(addr, ptr, len, trans.get_command(), sw, be, be_len);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);

        // ── Send END_REQ backward — Clause 15.2.4 ────────────────
        sc_core::sc_time bw_delay = sc_core::SC_ZERO_TIME;
        tlm::tlm_phase bw_phase = tlm::END_REQ;
        tlm::tlm_sync_enum bw_rc = socket->nb_transport_bw(trans, bw_phase, bw_delay);

        // Clause 15.2.2: END_REQ must be ACCEPTED or UPDATED
        assert(bw_rc == tlm::TLM_ACCEPTED || bw_rc == tlm::TLM_UPDATED);

        // ── Schedule BEGIN_RESP after access latency ─────────────
        sc_core::sc_time resp_latency = (trans.get_command() == tlm::TLM_READ_COMMAND)
                                            ? read_latency : write_latency;
        peq.notify(trans, tlm::BEGIN_RESP, resp_latency);
    }
    else if (phase == tlm::BEGIN_RESP)
    {
        // ── Send BEGIN_RESP backward ─────────────────────────────
        sc_core::sc_time bw_delay = sc_core::SC_ZERO_TIME;
        tlm::tlm_phase bw_phase = tlm::BEGIN_RESP;
        tlm::tlm_sync_enum bw_rc = socket->nb_transport_bw(trans, bw_phase, bw_delay);

        // Clause 15.2.5: initiator returns END_RESP immediately (response exclusion)
        // The TLM_UPDATED return carries the END_RESP phase back to us
        if (bw_rc == tlm::TLM_UPDATED && bw_phase == tlm::END_RESP)
        {
            // Initiator sent END_RESP inline — process it immediately
            peq.notify(trans, tlm::END_RESP, sc_core::SC_ZERO_TIME);
        }
    }
    else if (phase == tlm::END_RESP)
    {
        // ── Transaction complete — release refcount ──────────────
        trans.release();  // Clause 14.6: last release triggers SoCMM::free()
    }
}

// ═══════════════════════════════════════════════════════════════════════
// DMI — Clause 11.3 ────────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════
bool Memory::get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                 tlm::tlm_dmi& dmi_data)
{
    (void)trans;  // DMI grants full access regardless of transaction

    dmi_data.set_dmi_ptr(mem);
    dmi_data.set_start_address(0);       // will be translated downstream
    dmi_data.set_end_address(SIZE - 1);
    dmi_data.allow_read_write();
    dmi_data.set_read_latency(read_latency);
    dmi_data.set_write_latency(write_latency);

    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Debug transport — Clause 11.4 ─────────────────────────────────────────
//
// Direct memory access without advancing simulation time.
// No wait() calls allowed.
// ═══════════════════════════════════════════════════════════════════════
unsigned int Memory::transport_dbg(tlm::tlm_generic_payload& trans)
{
    uint64_t addr = trans.get_address();
    uint32_t offset = toOffset(addr);
    unsigned char* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();

    if (offset + len > SIZE)
    {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return 0;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND)
        std::memcpy(ptr, mem + offset, len);
    else if (trans.get_command() == tlm::TLM_WRITE_COMMAND)
        std::memcpy(mem + offset, ptr, len);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    return len;
}

// ─── Intel HEX loader ──────────────────────────────────────────────

static uint8_t hexChar(uint8_t c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

uint32_t Memory::loadHex(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Cannot open HEX file: " << filename << std::endl;
        return 0x80000000;
    }

    std::string line;
    uint32_t ext_addr = 0;
    uint32_t first_addr = 0;
    uint32_t start_pc = 0x80000000;
    bool first_data = true;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] != ':') continue;

        size_t len = hexChar(line[1]) * 16 + hexChar(line[2]);
        uint32_t addr = (hexChar(line[3]) << 12) | (hexChar(line[4]) << 8) |
                        (hexChar(line[5]) << 4) | hexChar(line[6]);
        uint8_t type = hexChar(line[7]) * 16 + hexChar(line[8]);

        if (type == 0x00)
        {
            uint32_t full_addr = addr + ext_addr;
            if (first_data)
            {
                base_addr = full_addr;
                first_addr = full_addr;
                first_data = false;
            }
            uint32_t offset = full_addr - base_addr;
            for (size_t i = 0; i < len && (offset + i) < SIZE; i++)
            {
                size_t pos = 9 + i * 2;
                mem[offset + i] = hexChar(line[pos]) * 16 + hexChar(line[pos + 1]);
            }
        }
        else if (type == 0x04)
        {
            ext_addr = 0;
            for (size_t i = 0; i < len; i++)
            {
                size_t pos = 9 + i * 2;
                ext_addr = (ext_addr << 8) | (hexChar(line[pos]) * 16 + hexChar(line[pos + 1]));
            }
            ext_addr <<= 16;
        }
        else if (type == 0x01)
        {
            break;
        }
    }

    if (!first_data) start_pc = first_addr;

    std::cout << "HEX loaded: " << filename << ", base=0x" << std::hex << base_addr
              << ", entry PC=0x" << start_pc << std::dec << std::endl;

    return start_pc;
}

// ─── ELF loader ─────────────────────────────────────────────────────

uint32_t Memory::loadELF(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Cannot open ELF file: " << filename << std::endl;
        return 0x80000000;
    }

    // Read ELF header (52 bytes for 32-bit)
    unsigned char ehdr[52];
    file.read(reinterpret_cast<char*>(ehdr), sizeof(ehdr));

    // Verify ELF magic
    if (ehdr[0] != 0x7F || ehdr[1] != 'E' || ehdr[2] != 'L' || ehdr[3] != 'F')
    {
        std::cerr << "Not a valid ELF file: " << filename << std::endl;
        return 0x80000000;
    }

    if (ehdr[4] != 1) // ELFCLASS32
    {
        std::cerr << "ELF is not 32-bit: " << filename << std::endl;
        return 0x80000000;
    }

    // Entry point
    uint32_t entry = read32LE(ehdr + 24);

    // Program header offset, entry size, count
    uint32_t phoff = read32LE(ehdr + 28);
    uint16_t phentsize = ehdr[42] | (ehdr[43] << 8);
    uint16_t phnum = ehdr[44] | (ehdr[45] << 8);

    // Read program headers
    for (uint16_t i = 0; i < phnum; i++)
    {
        uint32_t phdr_off = phoff + i * phentsize;
        file.seekg(phdr_off);
        unsigned char phdr[32];
        file.read(reinterpret_cast<char*>(phdr), sizeof(phdr));

        uint32_t p_type = read32LE(phdr);
        if (p_type != 1) continue; // PT_LOAD only

        uint32_t p_offset = read32LE(phdr + 4);
        uint32_t p_vaddr = read32LE(phdr + 8);
        uint32_t p_filesz = read32LE(phdr + 16);
        uint32_t p_memsz = read32LE(phdr + 20);

        uint32_t offset = toOffset(p_vaddr);
        if (offset + p_memsz > SIZE)
        {
            std::cerr << "ELF segment exceeds memory: vaddr=0x" << std::hex << p_vaddr
                      << " memsz=0x" << p_memsz << std::dec << std::endl;
            return entry;
        }

        // Copy segment data
        if (p_filesz > 0)
        {
            file.seekg(p_offset);
            file.read(reinterpret_cast<char*>(mem + offset), p_filesz);
        }
        // Zero-fill .bss
        if (p_memsz > p_filesz)
        {
            std::memset(mem + offset + p_filesz, 0, p_memsz - p_filesz);
        }

        if (i == 0)
        {
            base_addr = p_vaddr;
        }
    }

    std::cout << "ELF loaded: " << filename << ", base=0x" << std::hex << base_addr
              << ", entry PC=0x" << entry << std::dec << std::endl;

    return entry;
}

}  // namespace riscv_soc_tlm
