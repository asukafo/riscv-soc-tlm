#include "interconnect/interconnect.h"

#include <cassert>
#include <iostream>

namespace riscv_soc_tlm
{

Interconnect::Interconnect(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      targ_socket("targ_socket"),
      init_socket("init_socket")
{
    // ── Forward path registrations (upstream → downstream) ────────
    targ_socket.register_b_transport(this, &Interconnect::b_transport);
    targ_socket.register_nb_transport_fw(this, &Interconnect::nb_transport_fw);
    targ_socket.register_get_direct_mem_ptr(this, &Interconnect::get_direct_mem_ptr);
    targ_socket.register_transport_dbg(this, &Interconnect::transport_dbg);

    // ── Backward path registrations (downstream → upstream) ──────
    init_socket.register_nb_transport_bw(this, &Interconnect::nb_transport_bw);
    init_socket.register_invalidate_direct_mem_ptr(this, &Interconnect::invalidate_direct_mem_ptr);
}

int Interconnect::add_target()
{
    return next_port++;
}

void Interconnect::map(uint64_t base, uint64_t size, int port)
{
    regions.push_back({base, size, port});
}

int Interconnect::find_port(uint64_t addr) const
{
    for (auto& region : regions)
    {
        if (addr >= region.base && addr < region.base + region.size)
            return region.port;
    }
    return -1;
}

// ═══════════════════════════════════════════════════════════════════════
// LT path — blocking b_transport
// Clause 14.16: save/restore absolute address around translation
// ═══════════════════════════════════════════════════════════════════════
void Interconnect::b_transport(int /*id*/, tlm::tlm_generic_payload& trans,
                                sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();

    for (auto& region : regions)
    {
        if (addr >= region.base && addr < region.base + region.size)
        {
            // Clause 14.16: translate to slave-relative address
            uint64_t orig_addr = addr;
            uint64_t offset = addr - region.base;
            trans.set_address(offset);

            (*init_socket[region.port]).b_transport(trans, delay);

            // Clause 14.16: restore absolute address
            trans.set_address(orig_addr);
            return;
        }
    }

    std::cerr << "Interconnect: no target at address 0x" << std::hex << addr
              << std::dec << std::endl;
    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
}

// ═══════════════════════════════════════════════════════════════════════
// AT path — non-blocking nb_transport_fw — Clause 15.2
//
// Saves orig_addr + master_id in HopCtx, translates address to
// slave-relative offset, forwards to downstream target.
// ═══════════════════════════════════════════════════════════════════════
tlm::tlm_sync_enum Interconnect::nb_transport_fw(int id,
                                                   tlm::tlm_generic_payload& trans,
                                                   tlm::tlm_phase& phase,
                                                   sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();
    int port = find_port(addr);

    if (port < 0)
    {
        std::cerr << "Interconnect nb_fw: no target at 0x" << std::hex << addr
                  << std::dec << std::endl;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return tlm::TLM_COMPLETED;
    }

    if (phase == tlm::BEGIN_REQ)
    {
        // Find the region base for this port to compute offset
        uint64_t region_base = 0;
        for (auto& region : regions)
        {
            if (addr >= region.base && addr < region.base + region.size)
            {
                region_base = region.base;
                break;
            }
        }

        // Clause 14.16: save origin in hop context
        HopCtx* ctx = new HopCtx();
        ctx->orig_addr = addr;
        ctx->master_id = id;
        accessor(trans).set_extension(ctx);

        // Translate to slave-relative address
        uint64_t offset = addr - region_base;
        trans.set_address(offset);
    }

    // Forward to downstream target
    tlm::tlm_sync_enum rc = (*init_socket[port]).nb_transport_fw(trans, phase, delay);

    return rc;  // passthrough: TLM_ACCEPTED / TLM_UPDATED / TLM_COMPLETED
}

// ═══════════════════════════════════════════════════════════════════════
// AT backward path — nb_transport_bw — Clause 15.2
//
// Runs when downstream target sends a response backwards.
// Restores absolute address from HopCtx and forwards to the correct
// upstream initiator.
// ═══════════════════════════════════════════════════════════════════════
tlm::tlm_sync_enum Interconnect::nb_transport_bw(int /*port*/,
                                                   tlm::tlm_generic_payload& trans,
                                                   tlm::tlm_phase& phase,
                                                   sc_core::sc_time& delay)
{
    int master_id = 0;

    if (phase == tlm::BEGIN_RESP)
    {
        // Clause 14.16: restore absolute address from hop context
        HopCtx* ctx = nullptr;
        accessor(trans).get_extension(ctx);
        if (ctx)
        {
            trans.set_address(ctx->orig_addr);
            master_id = ctx->master_id;
            // NOTE: HopCtx is an instance_specific_extension; it will
            // be cleaned up when the trans is reset() by SoCMM::free().
        }
    }
    else if (phase == tlm::END_REQ)
    {
        // END_REQ also uses HopCtx to route back to correct master
        HopCtx* ctx = nullptr;
        accessor(trans).get_extension(ctx);
        if (ctx)
            master_id = ctx->master_id;
    }

    // Forward backward path to the correct upstream initiator
    return (*targ_socket[master_id]).nb_transport_bw(trans, phase, delay);
}

// ═══════════════════════════════════════════════════════════════════════
// DMI get_direct_mem_ptr — Clause 11.3
//
// Forward to downstream target, then translate DMI addresses from
// slave-relative back to absolute for the upstream initiator.
// ═══════════════════════════════════════════════════════════════════════
bool Interconnect::get_direct_mem_ptr(int /*id*/,
                                       tlm::tlm_generic_payload& trans,
                                       tlm::tlm_dmi& dmi_data)
{
    uint64_t addr = trans.get_address();

    for (auto& region : regions)
    {
        if (addr >= region.base && addr < region.base + region.size)
        {
            uint64_t offset = addr - region.base;
            trans.set_address(offset);

            bool ok = (*init_socket[region.port]).get_direct_mem_ptr(trans, dmi_data);

            // Clause 14.16: translate DMI addresses back to absolute
            if (ok)
            {
                dmi_data.set_start_address(dmi_data.get_start_address() + region.base);
                dmi_data.set_end_address(dmi_data.get_end_address() + region.base);
            }

            trans.set_address(addr);  // restore original
            return ok;
        }
    }

    return false;
}

// ═══════════════════════════════════════════════════════════════════════
// Debug transport — Clause 11.4
//
// Address translation + forward to downstream target.
// No wait() calls, no time advance.
// ═══════════════════════════════════════════════════════════════════════
unsigned int Interconnect::transport_dbg(int /*id*/,
                                          tlm::tlm_generic_payload& trans)
{
    uint64_t addr = trans.get_address();

    for (auto& region : regions)
    {
        if (addr >= region.base && addr < region.base + region.size)
        {
            uint64_t orig_addr = addr;
            trans.set_address(addr - region.base);

            unsigned int n = (*init_socket[region.port]).transport_dbg(trans);

            trans.set_address(orig_addr);
            return n;
        }
    }

    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════
// DMI invalidate broadcast — Clause 11.3.6
//
// When a downstream target invalidates a DMI region, translate from
// slave-relative to absolute address and broadcast to ALL upstream
// initiators.
// ═══════════════════════════════════════════════════════════════════════
void Interconnect::invalidate_direct_mem_ptr(int port,
                                              sc_dt::uint64 start,
                                              sc_dt::uint64 end)
{
    // Translate from slave-relative back to absolute
    uint64_t base = 0;
    for (auto& region : regions)
    {
        if (region.port == port)
        {
            base = region.base;
            break;
        }
    }

    sc_dt::uint64 abs_start = start + base;
    sc_dt::uint64 abs_end   = end   + base;

    // Clause 11.3.6: broadcast to all upstream initiators
    for (int i = 0; i < static_cast<int>(targ_socket.size()); i++)
    {
        (*targ_socket[i]).invalidate_direct_mem_ptr(abs_start, abs_end);
    }
}

}  // namespace riscv_soc_tlm
