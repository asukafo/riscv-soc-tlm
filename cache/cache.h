#ifndef __CACHE_H__
#define __CACHE_H__

#include <cstdint>
#include <vector>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/multi_passthrough_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"

namespace riscv_soc_tlm
{

struct CacheConfig
{
    uint32_t size;          // total cache size in bytes
    uint32_t line_size;     // bytes per cache line
    uint32_t associativity; // 1 = direct-mapped, N = N-way set-associative

    uint32_t num_lines() const { return size / line_size; }
    uint32_t num_sets() const { return num_lines() / associativity; }
};

class Cache : public sc_core::sc_module
{
public:
    tlm_utils::multi_passthrough_target_socket<Cache> target_socket;
    tlm_utils::simple_initiator_socket<Cache> initiator_socket;

    Cache(sc_core::sc_module_name name, const CacheConfig& config);

    uint64_t hits() const { return m_hits; }
    uint64_t misses() const { return m_misses; }
    uint64_t mmio_bypass() const { return m_mmio_bypass; }
    void reset_stats();

    // Add an MMIO range to bypass (start, end inclusive)
    void add_mmio_bypass(uint64_t start, uint64_t end);

private:
    struct Line
    {
        bool valid;
        uint32_t tag;
        std::vector<uint8_t> data;
        uint32_t lru; // higher = more recently used
    };

    CacheConfig m_config;
    std::vector<std::vector<Line>> m_sets; // sets[set_index][way]
    uint64_t m_hits;
    uint64_t m_misses;
    uint64_t m_mmio_bypass;  // count of accesses bypassed (MMIO)

    // MMIO bypass ranges: {start, end} pairs (inclusive)
    std::vector<std::pair<uint64_t, uint64_t>> m_mmio_ranges;

    bool is_mmio(uint64_t addr) const;

    // LT path (blocking, cached)
    void b_transport(int id, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // AT path (non-blocking, passthrough without caching)
    tlm::tlm_sync_enum nb_transport_fw(int id, tlm::tlm_generic_payload& trans,
                                        tlm::tlm_phase& phase,
                                        sc_core::sc_time& delay);

    // Debug transport — Clause 11.4
    unsigned int transport_dbg(int id, tlm::tlm_generic_payload& trans);

    // Internal LT helpers
    uint32_t set_index(uint64_t addr) const;
    uint32_t tag(uint64_t addr) const;
    uint32_t line_offset(uint64_t addr) const;

    int find_line(uint32_t set, uint32_t tag_val) const;
    int victim_way(uint32_t set) const;
    void fill_line(uint32_t set, int way, uint64_t addr);

    void read_hit(int set, int way, tlm::tlm_generic_payload& trans, uint32_t offset);
    void write_hit(int set, int way, tlm::tlm_generic_payload& trans, uint32_t offset);
};

} // namespace riscv_soc_tlm

#endif
