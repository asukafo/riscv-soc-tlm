#include "periph/fpu/fpu.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace riscv_soc_tlm
{

// Operation codes
enum : uint32_t
{
    OP_FADD      = 0x00,
    OP_FSUB      = 0x01,
    OP_FMUL      = 0x02,
    OP_FDIV      = 0x03,
    OP_FSQRT     = 0x04,
    OP_FCVT_W_S  = 0x05,
    OP_FCVT_S_W  = 0x06,
    OP_FMIN      = 0x07,
    OP_FMAX      = 0x08,
    OP_FEQ       = 0x09,
    OP_FLT       = 0x0A,
    OP_FLE       = 0x0B,
    OP_FMADD     = 0x0C,
};

// Status register bits
enum : uint32_t
{
    STATUS_BUSY      = 1u << 0,
    STATUS_DONE      = 1u << 1,
    STATUS_EXCEPTION = 1u << 2,
    STATUS_CAUSE_SHIFT = 3,
    STATUS_CAUSE_MASK  = 0x1Fu << 3,
};

// Exception cause codes
enum : int
{
    EXC_NONE           = 0,
    EXC_INVALID_OP     = 1,
    EXC_DIV_BY_ZERO    = 2,
    EXC_OVERFLOW       = 3,
    EXC_UNDERFLOW      = 4,
    EXC_INEXACT        = 5,
};

// Control register bits
enum : uint32_t
{
    CTRL_START           = 1u << 0,
    CTRL_CLEAR_EXCEPTION = 1u << 1,
};

SC_HAS_PROCESS(FPU);

FPU::FPU(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      target_socket("target_socket"),
      m_op_a(0),
      m_op_b(0),
      m_op_c(0),
      m_operation(0),
      m_result(0),
      m_status(0),
      m_ctrl(0)
{
    target_socket.register_b_transport(this, &FPU::b_transport);
    SC_THREAD(compute_thread);
}

void FPU::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    uint64_t addr = trans.get_address();
    unsigned char* data = trans.get_data_ptr();
    unsigned int len = trans.get_data_length();

    delay = sc_core::SC_ZERO_TIME;

    if (trans.get_command() == tlm::TLM_READ_COMMAND)
    {
        uint32_t value = 0;
        switch (addr & 0xFF)
        {
            case 0x00: value = m_op_a;      break;
            case 0x04: value = m_op_b;      break;
            case 0x08: value = m_operation; break;
            case 0x0C: value = m_result;    break;
            case 0x10: value = m_status;    break;
            case 0x14: value = m_ctrl;      break;
            case 0x18: value = m_op_c;      break;
            default:   value = 0;           break;
        }
        std::memcpy(data, &value, std::min(len, 4u));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    else if (trans.get_command() == tlm::TLM_WRITE_COMMAND)
    {
        uint32_t value = 0;
        std::memcpy(&value, data, std::min(len, 4u));

        switch (addr & 0xFF)
        {
            case 0x00: m_op_a      = value; break;
            case 0x04: m_op_b      = value; break;
            case 0x08: m_operation = value; break;
            case 0x14: m_ctrl      = value; break;
            case 0x18: m_op_c      = value; break;
            // 0x0C (RESULT) and 0x10 (STATUS) are read-only
            default: /* silently ignore */ break;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
}

void FPU::compute_thread()
{
    while (true)
    {
        // Handle CLEAR_EXCEPTION request
        if (m_ctrl & CTRL_CLEAR_EXCEPTION)
        {
            m_status &= ~(STATUS_EXCEPTION | STATUS_CAUSE_MASK);
            m_ctrl &= ~CTRL_CLEAR_EXCEPTION;
        }

        // Handle START request
        if (m_ctrl & CTRL_START)
        {
            m_status |= STATUS_BUSY;       // set busy
            m_status &= ~STATUS_DONE;      // clear done

            float a, b, c, result_f = 0.0f;
            int32_t result_i = 0;
            int exception_cause = EXC_NONE;
            bool div_by_zero = false;
            bool is_int_result = false;

            std::memcpy(&a, &m_op_a, sizeof(a));
            std::memcpy(&b, &m_op_b, sizeof(b));
            std::memcpy(&c, &m_op_c, sizeof(c));

            switch (m_operation)
            {
                case OP_FADD:
                    result_f = a + b;
                    break;

                case OP_FSUB:
                    result_f = a - b;
                    break;

                case OP_FMUL:
                    result_f = a * b;
                    break;

                case OP_FDIV:
                    if (b == 0.0f && a != 0.0f)
                        div_by_zero = true;
                    result_f = a / b;
                    break;

                case OP_FSQRT:
                    if (a < 0.0f)
                    {
                        result_f = std::sqrt(a);  // produces NaN
                        exception_cause = EXC_INVALID_OP;
                    }
                    else
                    {
                        result_f = std::sqrt(a);
                    }
                    break;

                case OP_FCVT_W_S:
                    result_i = static_cast<int32_t>(a);
                    is_int_result = true;
                    break;

                case OP_FCVT_S_W:
                    result_f = static_cast<float>(static_cast<int32_t>(m_op_a));
                    break;

                case OP_FMIN:
                    result_f = std::fmin(a, b);
                    break;

                case OP_FMAX:
                    result_f = std::fmax(a, b);
                    break;

                case OP_FEQ:
                    result_i = (a == b) ? 1 : 0;
                    is_int_result = true;
                    break;

                case OP_FLT:
                    result_i = (a < b) ? 1 : 0;
                    is_int_result = true;
                    break;

                case OP_FLE:
                    result_i = (a <= b) ? 1 : 0;
                    is_int_result = true;
                    break;

                case OP_FMADD:
                    result_f = std::fma(a, b, c);
                    break;

                default:
                    // Unknown operation — result stays 0, no exception
                    break;
            }

            // Detect exceptions on float results (if not already detected)
            if (exception_cause == EXC_NONE && !is_int_result && !div_by_zero)
            {
                if (std::isnan(result_f))
                    exception_cause = EXC_INVALID_OP;
                else if (std::isinf(result_f))
                    exception_cause = EXC_OVERFLOW;
                else if (result_f != 0.0f && std::fpclassify(result_f) == FP_SUBNORMAL)
                    exception_cause = EXC_UNDERFLOW;
            }
            if (div_by_zero)
                exception_cause = EXC_DIV_BY_ZERO;

            // Store result
            if (is_int_result)
            {
                m_result = static_cast<uint32_t>(result_i);
            }
            else
            {
                std::memcpy(&m_result, &result_f, sizeof(m_result));
            }

            // Update status
            m_status &= ~STATUS_BUSY;                        // clear busy
            m_status |= STATUS_DONE;                          // set done
            m_status &= ~STATUS_CAUSE_MASK;                   // clear old cause
            if (exception_cause != EXC_NONE)
            {
                m_status |= STATUS_EXCEPTION;                            // set exception flag
                m_status |= (exception_cause << STATUS_CAUSE_SHIFT);     // set cause
            }

            m_ctrl &= ~CTRL_START;  // auto-clear start bit
        }

        sc_core::wait(100, sc_core::SC_NS);
    }
}

}  // namespace riscv_soc_tlm
