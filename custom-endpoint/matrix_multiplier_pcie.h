#ifndef MATRIX_MULTIPLIER_PCIE_H
#define MATRIX_MULTIPLIER_PCIE_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

// Register Map for BAR0
#define REG_CONTROL 0x0000      // Control register
#define REG_STATUS 0x0004       // Status register
#define REG_DIM_N 0x0008        // Matrix dimension N
#define REG_MATRIX_A_PTR 0x0010 // DMA pointer to Matrix A (host memory)
#define REG_MATRIX_B_PTR 0x0018 // DMA pointer to Matrix B (host memory)
#define REG_MATRIX_C_PTR 0x0020 // DMA pointer to Matrix C (host memory)
#define REG_INT_STATUS 0x0028   // Interrupt status
#define REG_INT_ENABLE 0x002C   // Interrupt enable

// Control Register Bits
#define CTRL_START (1 << 0) // Start computation
#define CTRL_RESET (1 << 1) // Reset device

// Status Register Bits
#define STATUS_IDLE (1 << 0)  // Device is idle
#define STATUS_BUSY (1 << 1)  // Device is busy
#define STATUS_DONE (1 << 2)  // Computation done
#define STATUS_ERROR (1 << 3) // Error occurred

// Interrupt Bits
#define INT_DONE (1 << 0) // Computation done interrupt

SC_MODULE(matrix_multiplier_pcie)
{
public:
    // TLM sockets
    tlm_utils::simple_target_socket<matrix_multiplier_pcie> bar0_target_socket;
    tlm_utils::simple_initiator_socket<matrix_multiplier_pcie> dma_initiator_socket;

    // Interrupt output
    sc_out<bool> interrupt;

    SC_CTOR(matrix_multiplier_pcie)
        : bar0_target_socket("bar0_target_socket"),
          dma_initiator_socket("dma_initiator_socket"),
          interrupt("interrupt")
    {
        // Register TLM callbacks
        bar0_target_socket.register_b_transport(this, &matrix_multiplier_pcie::bar0_b_transport);

        // Initialize registers
        reset_device();

        // Spawn computation thread
        SC_THREAD(compute_thread);
    }

private:
    // Device registers
    uint32_t reg_control;
    uint32_t reg_status;
    uint32_t reg_dim_n;
    uint64_t reg_matrix_a_ptr;
    uint64_t reg_matrix_b_ptr;
    uint64_t reg_matrix_c_ptr;
    uint32_t reg_int_status;
    uint32_t reg_int_enable;

    // Internal signals
    sc_event start_event;
    bool computation_requested;

    // Reset device state
    void reset_device()
    {
        reg_control = 0;
        reg_status = STATUS_IDLE;
        reg_dim_n = 0;
        reg_matrix_a_ptr = 0;
        reg_matrix_b_ptr = 0;
        reg_matrix_c_ptr = 0;
        reg_int_status = 0;
        reg_int_enable = 0;
        computation_requested = false;
        interrupt.write(false);
    }

    // BAR0 target socket handler (MMIO access from host)
    void bar0_b_transport(tlm::tlm_generic_payload & trans, sc_time & delay)
    {
        tlm::tlm_command cmd = trans.get_command();
        sc_dt::uint64 addr = trans.get_address();
        unsigned char *ptr = trans.get_data_ptr();
        unsigned int len = trans.get_data_length();

        if (len != 4 && len != 8)
        {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        if (cmd == tlm::TLM_READ_COMMAND)
        {
            handle_mmio_read(addr, ptr, len);
        }
        else if (cmd == tlm::TLM_WRITE_COMMAND)
        {
            handle_mmio_write(addr, ptr, len);
        }

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_time(10, SC_NS); // Simulated register access time
    }

    // Handle MMIO reads
    void handle_mmio_read(uint64_t addr, unsigned char *data, unsigned int len)
    {
        uint32_t value = 0;

        switch (addr)
        {
        case REG_CONTROL:
            value = reg_control;
            break;
        case REG_STATUS:
            value = reg_status;
            break;
        case REG_DIM_N:
            value = reg_dim_n;
            break;
        case REG_MATRIX_A_PTR:
            if (len == 8)
            {
                memcpy(data, &reg_matrix_a_ptr, 8);
                return;
            }
            value = (uint32_t)reg_matrix_a_ptr;
            break;
        case REG_MATRIX_A_PTR + 4:
            value = (uint32_t)(reg_matrix_a_ptr >> 32);
            break;
        case REG_MATRIX_B_PTR:
            if (len == 8)
            {
                memcpy(data, &reg_matrix_b_ptr, 8);
                return;
            }
            value = (uint32_t)reg_matrix_b_ptr;
            break;
        case REG_MATRIX_B_PTR + 4:
            value = (uint32_t)(reg_matrix_b_ptr >> 32);
            break;
        case REG_MATRIX_C_PTR:
            if (len == 8)
            {
                memcpy(data, &reg_matrix_c_ptr, 8);
                return;
            }
            value = (uint32_t)reg_matrix_c_ptr;
            break;
        case REG_MATRIX_C_PTR + 4:
            value = (uint32_t)(reg_matrix_c_ptr >> 32);
            break;
        case REG_INT_STATUS:
            value = reg_int_status;
            break;
        case REG_INT_ENABLE:
            value = reg_int_enable;
            break;
        default:
            cout << "WARNING: Read from undefined register 0x" << hex << addr << endl;
            value = 0xDEADBEEF;
        }

        memcpy(data, &value, len);
    }

    // Handle MMIO writes
    void handle_mmio_write(uint64_t addr, unsigned char *data, unsigned int len)
    {
        uint32_t value;
        memcpy(&value, data, len);

        switch (addr)
        {
        case REG_CONTROL:
            reg_control = value;
            if (value & CTRL_RESET)
            {
                reset_device();
                cout << "[" << sc_time_stamp() << "] Device reset" << endl;
            }
            if (value & CTRL_START)
            {
                if (reg_status & STATUS_IDLE)
                {
                    computation_requested = true;
                    start_event.notify();
                    cout << "[" << sc_time_stamp() << "] Computation started" << endl;
                }
            }
            break;
        case REG_DIM_N:
            reg_dim_n = value;
            cout << "[" << sc_time_stamp() << "] Matrix dimension set to " << value << endl;
            break;
        case REG_MATRIX_A_PTR:
            if (len == 8)
            {
                memcpy(&reg_matrix_a_ptr, data, 8);
            }
            else
            {
                reg_matrix_a_ptr = (reg_matrix_a_ptr & 0xFFFFFFFF00000000ULL) | value;
            }
            break;
        case REG_MATRIX_A_PTR + 4:
            reg_matrix_a_ptr = (reg_matrix_a_ptr & 0xFFFFFFFF) | ((uint64_t)value << 32);
            break;
        case REG_MATRIX_B_PTR:
            if (len == 8)
            {
                memcpy(&reg_matrix_b_ptr, data, 8);
            }
            else
            {
                reg_matrix_b_ptr = (reg_matrix_b_ptr & 0xFFFFFFFF00000000ULL) | value;
            }
            break;
        case REG_MATRIX_B_PTR + 4:
            reg_matrix_b_ptr = (reg_matrix_b_ptr & 0xFFFFFFFF) | ((uint64_t)value << 32);
            break;
        case REG_MATRIX_C_PTR:
            if (len == 8)
            {
                memcpy(&reg_matrix_c_ptr, data, 8);
            }
            else
            {
                reg_matrix_c_ptr = (reg_matrix_c_ptr & 0xFFFFFFFF00000000ULL) | value;
            }
            break;
        case REG_MATRIX_C_PTR + 4:
            reg_matrix_c_ptr = (reg_matrix_c_ptr & 0xFFFFFFFF) | ((uint64_t)value << 32);
            break;
        case REG_INT_STATUS:
            // Write 1 to clear
            reg_int_status &= ~value;
            update_interrupt();
            break;
        case REG_INT_ENABLE:
            reg_int_enable = value;
            update_interrupt();
            break;
        default:
            cout << "WARNING: Write to undefined register 0x" << hex << addr << endl;
        }
    }

    // DMA read from host memory
    bool dma_read(uint64_t addr, unsigned char *data, unsigned int len)
    {
        tlm::tlm_generic_payload trans;
        sc_time delay = sc_time(50, SC_NS); // DMA latency

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(data);
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        dma_initiator_socket->b_transport(trans, delay);
        wait(delay);

        return trans.is_response_ok();
    }

    // DMA write to host memory
    bool dma_write(uint64_t addr, unsigned char *data, unsigned int len)
    {
        tlm::tlm_generic_payload trans;
        sc_time delay = sc_time(50, SC_NS); // DMA latency

        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(data);
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        dma_initiator_socket->b_transport(trans, delay);
        wait(delay);

        return trans.is_response_ok();
    }

    // Update interrupt line
    void update_interrupt()
    {
        bool irq = (reg_int_status & reg_int_enable) != 0;
        interrupt.write(irq);
    }

    // Main computation thread
    void compute_thread()
    {
        while (true)
        {
            wait(start_event);

            if (!computation_requested)
                continue;

            reg_status = STATUS_BUSY;
            reg_status &= ~STATUS_IDLE;
            computation_requested = false;

            cout << "[" << sc_time_stamp() << "] Starting matrix multiplication (N="
                 << reg_dim_n << ")" << endl;

            // Perform matrix multiplication
            bool success = perform_matrix_multiply();

            // Update status
            if (success)
            {
                reg_status = STATUS_IDLE | STATUS_DONE;
                reg_int_status |= INT_DONE;
                cout << "[" << sc_time_stamp() << "] Computation completed successfully" << endl;
            }
            else
            {
                reg_status = STATUS_IDLE | STATUS_ERROR;
                cout << "[" << sc_time_stamp() << "] Computation failed!" << endl;
            }

            update_interrupt();
        }
    }

    // Actual matrix multiplication
    bool perform_matrix_multiply()
    {
        uint32_t n = reg_dim_n;

        if (n == 0 || n > 256)
        { // Sanity check
            cout << "ERROR: Invalid matrix dimension" << endl;
            return false;
        }

        // Allocate local buffers
        vector<float> matrix_a(n * n);
        vector<float> matrix_b(n * n);
        vector<float> matrix_c(n * n, 0.0f);

        // DMA read Matrix A
        cout << "  Reading Matrix A from 0x" << hex << reg_matrix_a_ptr << endl;
        if (!dma_read(reg_matrix_a_ptr, (unsigned char *)matrix_a.data(), n * n * sizeof(float)))
        {
            cout << "ERROR: Failed to read Matrix A" << endl;
            return false;
        }

        // DMA read Matrix B
        cout << "  Reading Matrix B from 0x" << hex << reg_matrix_b_ptr << endl;
        if (!dma_read(reg_matrix_b_ptr, (unsigned char *)matrix_b.data(), n * n * sizeof(float)))
        {
            cout << "ERROR: Failed to read Matrix B" << endl;
            return false;
        }

        // Perform multiplication: C = A * B
        cout << "  Computing C = A * B..." << endl;
        for (uint32_t i = 0; i < n; i++)
        {
            for (uint32_t j = 0; j < n; j++)
            {
                float sum = 0.0f;
                for (uint32_t k = 0; k < n; k++)
                {
                    sum += matrix_a[i * n + k] * matrix_b[k * n + j];
                }
                matrix_c[i * n + j] = sum;
            }
            // Simulate computation time
            wait(sc_time(n * 2, SC_NS));
        }

        // DMA write Matrix C
        cout << "  Writing Matrix C to 0x" << hex << reg_matrix_c_ptr << endl;
        if (!dma_write(reg_matrix_c_ptr, (unsigned char *)matrix_c.data(), n * n * sizeof(float)))
        {
            cout << "ERROR: Failed to write Matrix C" << endl;
            return false;
        }

        return true;
    }
};

#endif // MATRIX_MULTIPLIER_PCIE_H