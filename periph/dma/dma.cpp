#include "periph/dma/dma.h"

#include <cassert>
#include <cstring>
#include <iostream>

namespace riscv_soc_tlm
{

SC_HAS_PROCESS(DMA);

DMA::DMA(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      target_socket("target_socket"),
      initiator_socket("initiator_socket"),
      m_src_addr(0),
      m_dst_addr(0),
      m_size(0),
      m_ctrl(0)
{
    // LT: MMIO register access from CPU
    target_socket.register_b_transport(this, &DMA::b_transport);
    // Debug: Clause 11.4
    target_socket.register_transport_dbg(this, &DMA::transport_dbg);
    // AT: backward path for responses from memory
    initiator_socket.register_nb_transport_bw(this, &DMA::nb_transport_bw);

    SC_THREAD(transfer_thread);
}

// ═══════════════════════════════════════════════════════════════════════
// LT path — MMIO register access (unchanged)
// ═══════════════════════════════════════════════════════════════════════
void DMA::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();
    unsigned char* ptr = trans.get_data_ptr();
    uint32_t len = trans.get_data_length();
    // Clause 16.4: accumulate delay for quantum keeper
    delay += sc_core::sc_time(1, sc_core::SC_NS);  // MMIO response time

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND)
    {
        uint32_t value = 0;
        std::memcpy(&value, ptr, std::min(len, 4u));

        switch (addr & 0xFF)
        {
            case 0x00:  // SRC_LO — preserve upper 32 bits
                m_src_addr = (m_src_addr & 0xFFFFFFFF00000000ULL) |
                             (uint64_t(value) & 0xFFFFFFFFULL);
                break;
            case 0x04:  // SRC_HI — preserve lower 32 bits
                m_src_addr = (m_src_addr & 0x00000000FFFFFFFFULL) |
                             (uint64_t(value) << 32);
                break;
            case 0x08:  // DST_LO — preserve upper 32 bits
                m_dst_addr = (m_dst_addr & 0xFFFFFFFF00000000ULL) |
                             (uint64_t(value) & 0xFFFFFFFFULL);
                break;
            case 0x0C:  // DST_HI — preserve lower 32 bits
                m_dst_addr = (m_dst_addr & 0x00000000FFFFFFFFULL) |
                             (uint64_t(value) << 32);
                break;
            case 0x10:
                m_size = value;
                break;
            case 0x14:
                m_ctrl = value;
                break;
        }
    }
    else if (trans.get_command() == tlm::TLM_READ_COMMAND)
    {
        uint32_t value = 0;
        switch (addr & 0xFF)
        {
            case 0x00: value = static_cast<uint32_t>(m_src_addr); break;
            case 0x04: value = static_cast<uint32_t>(m_src_addr >> 32); break;
            case 0x08: value = static_cast<uint32_t>(m_dst_addr); break;
            case 0x0C: value = static_cast<uint32_t>(m_dst_addr >> 32); break;
            case 0x10: value = m_size; break;
            case 0x14: value = m_ctrl; break;
        }
        std::memcpy(ptr, &value, std::min(len, 4u));
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

// ═══════════════════════════════════════════════════════════════════════
// Debug transport — Clause 11.4 (MMIO peek/poke)
// ═══════════════════════════════════════════════════════════════════════
unsigned int DMA::transport_dbg(tlm::tlm_generic_payload& trans)
{
    // Reuse b_transport logic for simple MMIO reads/writes
    sc_core::sc_time t = sc_core::SC_ZERO_TIME;
    b_transport(trans, t);
    return trans.get_data_length();
}

// ═══════════════════════════════════════════════════════════════════════
// AT backward path — Clause 15.2.4, 15.2.5
//
// Receives END_REQ (request exclusion satisfied) and BEGIN_RESP
// (response exclusion: return END_RESP immediately).
// ═══════════════════════════════════════════════════════════════════════
tlm::tlm_sync_enum DMA::nb_transport_bw(tlm::tlm_generic_payload& trans,
                                          tlm::tlm_phase& phase,
                                          sc_core::sc_time& delay)
{
    // Clause 15.2.2: validate backward-path phases
    assert(phase == tlm::END_REQ || phase == tlm::BEGIN_RESP);

    if (phase == tlm::END_REQ)
    {
        // Clause 15.2.4: request exclusion satisfied — can send next BEGIN_REQ
        m_end_req_event.notify(sc_core::SC_ZERO_TIME);
        delay = sc_core::SC_ZERO_TIME;
        return tlm::TLM_ACCEPTED;
    }

    if (phase == tlm::BEGIN_RESP)
    {
        // Clause 15.2.5: response exclusion — complete immediately
        if (trans.get_command() == tlm::TLM_READ_COMMAND)
        {
            // Capture read data from response
            std::memcpy(&m_read_data, trans.get_data_ptr(), 4);
        }
        else
        {
            m_write_done = true;
        }

        m_resp_event.notify(sc_core::SC_ZERO_TIME);  // wake AT thread
        phase = tlm::END_RESP;  // Response exclusion: MUST complete now
        delay = sc_core::SC_ZERO_TIME;
        return tlm::TLM_UPDATED;
    }

    return tlm::TLM_ACCEPTED;
}

// ═══════════════════════════════════════════════════════════════════════
// AT initiator helpers — Clause 14.6, 15.2.4, 15.2.5
//
// Each word transfer is a complete AT transaction:
//   BEGIN_REQ → END_REQ → BEGIN_RESP → END_RESP
// ═══════════════════════════════════════════════════════════════════════

uint32_t DMA::readWordAT(uint64_t addr)
{
    // Clause 14.6: allocate from MM pool
    m_at_trans = m_mm->allocate();
    m_at_trans->set_command(tlm::TLM_READ_COMMAND);
    m_at_trans->set_address(addr);
    m_at_trans->set_data_ptr(reinterpret_cast<unsigned char*>(&m_read_data));
    m_at_trans->set_data_length(4);
    m_at_trans->set_streaming_width(4);
    m_at_trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Clause 14.6: acquire refcount (initiator holds one)
    m_at_trans->acquire();

    // Send BEGIN_REQ
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    tlm::tlm_sync_enum rc = initiator_socket->nb_transport_fw(*m_at_trans, phase, delay);

    // Clause 15.2.2: validate return
    assert(phase == tlm::BEGIN_REQ || phase == tlm::END_REQ);
    assert(rc == tlm::TLM_ACCEPTED || rc == tlm::TLM_UPDATED);

    if (rc == tlm::TLM_UPDATED && phase == tlm::END_REQ)
    {
        // Target completed END_REQ inline — no wait needed
    }
    else
    {
        // Clause 15.2.4: wait for request exclusion (END_REQ)
        sc_core::wait(m_end_req_event);
    }

    // Wait for BEGIN_RESP (handled in nb_transport_bw)
    // nb_transport_bw captures data and notifies m_resp_event
    sc_core::wait(m_resp_event);

    uint32_t result = m_read_data;

    // Clause 14.6: yield so PEQ processes END_RESP before we release.
    // PEQ END_RESP callback decrements refcount 2→1.
    // Then our release decrements 1→0 → SoCMM::free().
    // Using 1ns (not SC_ZERO_TIME) ensures the scheduler advances
    // simulation time past the PEQ's scheduled callback time.
    sc_core::wait(1, sc_core::SC_NS);

    m_at_trans->release();
    m_at_trans = nullptr;

    return result;
}

void DMA::writeWordAT(uint64_t addr, uint32_t data)
{
    m_write_done = false;

    m_at_trans = m_mm->allocate();
    m_at_trans->set_command(tlm::TLM_WRITE_COMMAND);
    m_at_trans->set_address(addr);
    m_at_trans->set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    m_at_trans->set_data_length(4);
    m_at_trans->set_streaming_width(4);
    m_at_trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Clause 14.6
    m_at_trans->acquire();

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    tlm::tlm_sync_enum rc = initiator_socket->nb_transport_fw(*m_at_trans, phase, delay);

    if (!(rc == tlm::TLM_UPDATED && phase == tlm::END_REQ))
    {
        sc_core::wait(m_end_req_event);  // Clause 15.2.4
    }

    // Wait for BEGIN_RESP (handled in nb_transport_bw)
    sc_core::wait(m_resp_event);

    // Let PEQ process END_RESP before we release (Clause 14.6 ordering)
    sc_core::wait(1, sc_core::SC_NS);

    m_at_trans->release();
    m_at_trans = nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// Transfer thread — now uses AT path for data movement
// ═══════════════════════════════════════════════════════════════════════
void DMA::transfer_thread()
{
    while (true)
    {
        // Yield 10ns so SystemC can interleave CPU and DMA threads
        sc_core::wait(10, sc_core::SC_NS);

        if ((m_ctrl & 1) && m_mm != nullptr)
        {
            std::cout << "[DMA] AT Transfer " << m_size << " bytes from 0x"
                      << std::hex << m_src_addr << " to 0x" << m_dst_addr
                      << std::dec << std::endl;

            for (uint32_t offset = 0; offset < m_size; offset += 4)
            {
                uint32_t data = readWordAT(m_src_addr + offset);
                writeWordAT(m_dst_addr + offset, data);
            }

            std::cout << "[DMA] AT Transfer complete." << std::endl;
            m_ctrl = 0; // clear start bit
        }
    }
}

}  // namespace riscv_soc_tlm
