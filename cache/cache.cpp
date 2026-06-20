#include "cache/cache.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace riscv_soc_tlm
{

Cache::Cache(sc_core::sc_module_name name, const CacheConfig& config)
    : sc_core::sc_module(name),
      target_socket("target_socket"),
      initiator_socket("initiator_socket"),
      m_config(config),
      m_hits(0),
      m_misses(0),
      m_mmio_bypass(0)
{
    // LT path
    target_socket.register_b_transport(this, &Cache::b_transport);
    // AT path — passthrough without caching
    target_socket.register_nb_transport_fw(this, &Cache::nb_transport_fw);
    // Debug — Clause 11.4
    target_socket.register_transport_dbg(this, &Cache::transport_dbg);

    uint32_t nsets = config.num_sets();
    uint32_t nways = config.associativity;

    m_sets.resize(nsets);
    for (auto& set : m_sets)
    {
        set.resize(nways);
        for (auto& line : set)
        {
            line.valid = false;
            line.tag = 0;
            line.lru = 0;
            line.data.resize(config.line_size, 0);
        }
    }
}

void Cache::reset_stats()
{
    m_hits = 0;
    m_misses = 0;
    m_mmio_bypass = 0;
}

bool Cache::is_mmio(uint64_t addr) const
{
    for (auto& r : m_mmio_ranges)
    {
        if (addr >= r.first && addr <= r.second)
            return true;
    }
    return false;
}

void Cache::add_mmio_bypass(uint64_t start, uint64_t end)
{
    m_mmio_ranges.push_back({start, end});
}

uint32_t Cache::line_offset(uint64_t addr) const
{
    return addr & (m_config.line_size - 1);
}

uint32_t Cache::set_index(uint64_t addr) const
{
    return (addr / m_config.line_size) % m_config.num_sets();
}

uint32_t Cache::tag(uint64_t addr) const
{
    return addr / (m_config.line_size * m_config.num_sets());
}

int Cache::find_line(uint32_t set, uint32_t tag_val) const
{
    for (size_t i = 0; i < m_sets[set].size(); i++)
    {
        if (m_sets[set][i].valid && m_sets[set][i].tag == tag_val)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Cache::victim_way(uint32_t set) const
{
    // Find first invalid line
    for (size_t i = 0; i < m_sets[set].size(); i++)
    {
        if (!m_sets[set][i].valid) return static_cast<int>(i);
    }

    // All valid: LRU eviction — smallest lru counter
    int victim = 0;
    uint32_t min_lru = m_sets[set][0].lru;
    for (size_t i = 1; i < m_sets[set].size(); i++)
    {
        if (m_sets[set][i].lru < min_lru)
        {
            min_lru = m_sets[set][i].lru;
            victim = static_cast<int>(i);
        }
    }
    return victim;
}

void Cache::fill_line(uint32_t set, int way, uint64_t addr)
{
    Line& line = m_sets[set][way];

    // Read full line from downstream memory
    uint64_t line_addr = addr & ~(uint64_t(m_config.line_size) - 1);
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(line_addr);
    trans.set_data_ptr(line.data.data());
    trans.set_data_length(m_config.line_size);
    trans.set_streaming_width(m_config.line_size);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error())
    {
        std::cerr << "Cache fill error at addr=0x" << std::hex << line_addr
                  << std::dec << std::endl;
        return;
    }

    line.valid = true;
    line.tag = tag(line_addr);
}

void Cache::read_hit(int set, int way, tlm::tlm_generic_payload& trans,
                     uint32_t offset)
{
    Line& line = m_sets[set][way];
    line.lru = ++m_hits + m_misses; // advance LRU tick

    uint32_t len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();
    std::memcpy(ptr, line.data.data() + offset, len);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void Cache::write_hit(int set, int way, tlm::tlm_generic_payload& trans,
                      uint32_t offset)
{
    Line& line = m_sets[set][way];
    line.lru = ++m_hits + m_misses;

    uint32_t len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();
    std::memcpy(line.data.data() + offset, ptr, len);

    // Write-through: also forward to downstream
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    initiator_socket->b_transport(trans, delay);
}

// ═══════════════════════════════════════════════════════════════════════
// LT path — blocking b_transport (cached)
// ═══════════════════════════════════════════════════════════════════════
void Cache::b_transport(int /*id*/, tlm::tlm_generic_payload& trans,
                        sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();

    // MMIO bypass: peripherals are not cacheable — forward directly
    if (is_mmio(addr))
    {
        m_mmio_bypass++;
        initiator_socket->b_transport(trans, delay);
        return;
    }

    uint32_t set = set_index(addr);
    uint32_t tag_val = tag(addr);
    uint32_t offset = line_offset(addr);
    int way = find_line(set, tag_val);

    // Clause 16.4: preserve caller's delay for quantum keeper
    // (don't overwrite — downstream targets will add their latency)

    if (way >= 0 && trans.get_command() == tlm::TLM_READ_COMMAND)
    {
        m_hits++;
        read_hit(set, way, trans, offset);
    }
    else if (way >= 0 && trans.get_command() == tlm::TLM_WRITE_COMMAND)
    {
        m_hits++;
        write_hit(set, way, trans, offset);
    }
    else if (way < 0 && trans.get_command() == tlm::TLM_READ_COMMAND)
    {
        m_misses++;
        // Allocate on read miss
        int victim = victim_way(set);
        fill_line(set, victim, addr);
        way = find_line(set, tag_val);
        read_hit(set, way, trans, offset);
    }
    else // write miss: forward through without allocating
    {
        m_misses++;
        initiator_socket->b_transport(trans, delay);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// AT path — non-blocking passthrough (no caching for AT transactions)
//
// AT transactions bypass the cache and are forwarded directly to the
// downstream interconnect.  The cache is write-through anyway, so
// forwarding without caching is correct for both reads and writes.
// ═══════════════════════════════════════════════════════════════════════
tlm::tlm_sync_enum Cache::nb_transport_fw(int /*id*/,
                                            tlm::tlm_generic_payload& trans,
                                            tlm::tlm_phase& phase,
                                            sc_core::sc_time& delay)
{
    // AT passthrough: forward directly to downstream without caching
    return initiator_socket->nb_transport_fw(trans, phase, delay);
}

// ═══════════════════════════════════════════════════════════════════════
// Debug transport — Clause 11.4 (passthrough)
// ═══════════════════════════════════════════════════════════════════════
unsigned int Cache::transport_dbg(int /*id*/, tlm::tlm_generic_payload& trans)
{
    return initiator_socket->transport_dbg(trans);
}

} // namespace riscv_soc_tlm
