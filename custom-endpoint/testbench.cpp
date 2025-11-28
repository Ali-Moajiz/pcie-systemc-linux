#include <systemc.h>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include "matrix_multiplier_pcie.h"

using namespace sc_core;
using namespace std;

// Simple memory model to act as host memory
SC_MODULE(host_memory)
{
public:
    tlm_utils::simple_target_socket<host_memory> target_socket;

    SC_CTOR(host_memory) : target_socket("target_socket")
    {
        target_socket.register_b_transport(this, &host_memory::b_transport);

        // Allocate 16MB of host memory
        memory.resize(16 * 1024 * 1024, 0);
    }

    void b_transport(tlm::tlm_generic_payload & trans, sc_time & delay)
    {
        tlm::tlm_command cmd = trans.get_command();
        sc_dt::uint64 addr = trans.get_address();
        unsigned char *ptr = trans.get_data_ptr();
        unsigned int len = trans.get_data_length();

        if (addr + len > memory.size())
        {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (cmd == tlm::TLM_READ_COMMAND)
        {
            memcpy(ptr, &memory[addr], len);
        }
        else if (cmd == tlm::TLM_WRITE_COMMAND)
        {
            memcpy(&memory[addr], ptr, len);
        }

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_time(20, SC_NS);
    }

    // Helper to write data to memory
    void write_data(uint64_t addr, void *data, size_t len)
    {
        if (addr + len <= memory.size())
        {
            memcpy(&memory[addr], data, len);
        }
    }

    // Helper to read data from memory
    void read_data(uint64_t addr, void *data, size_t len)
    {
        if (addr + len <= memory.size())
        {
            memcpy(data, &memory[addr], len);
        }
    }

private:
    vector<unsigned char> memory;
};

// Test driver that simulates PCIe host
SC_MODULE(test_driver)
{
public:
    tlm_utils::simple_initiator_socket<test_driver> bar0_socket;
    sc_in<bool> interrupt;

    SC_CTOR(test_driver) : bar0_socket("bar0_socket")
    {
        SC_THREAD(test_sequence);
    }

private:
    void test_sequence()
    {
        cout << "\n[TEST] Starting PCIe device test" << endl;
        wait(100, SC_NS);

        // Test 1: Read device status
        cout << "\n[TEST] Reading device status..." << endl;
        uint32_t status = mmio_read32(0x0004);
        cout << "[TEST] Status = 0x" << hex << status << dec << endl;

        // Test 2: Configure matrix multiplication
        uint32_t n = 4; // 4x4 matrices
        cout << "\n[TEST] Configuring for " << n << "x" << n << " matrix multiplication" << endl;

        mmio_write32(0x0008, n);      // Set dimension
        mmio_write64(0x0010, 0x1000); // Matrix A at address 0x1000
        mmio_write64(0x0018, 0x2000); // Matrix B at address 0x2000
        mmio_write64(0x0020, 0x3000); // Matrix C at address 0x3000

        // Enable interrupts
        mmio_write32(0x002C, 0x1); // Enable DONE interrupt

        // Test 3: Start computation
        cout << "[TEST] Starting computation..." << endl;
        mmio_write32(0x0000, 0x1); // Set START bit

        // Wait for interrupt
        cout << "[TEST] Waiting for completion..." << endl;
        wait(interrupt.posedge_event());

        cout << "\n[TEST] Interrupt received!" << endl;

        // Read status
        status = mmio_read32(0x0004);
        cout << "[TEST] Final status = 0x" << hex << status << dec << endl;

        // Read interrupt status
        uint32_t int_status = mmio_read32(0x0028);
        cout << "[TEST] Interrupt status = 0x" << hex << int_status << dec << endl;

        // Clear interrupt
        mmio_write32(0x0028, int_status);

        cout << "\n[TEST] Test completed successfully!" << endl;

        wait(100, SC_NS);
        sc_stop();
    }

    // Helper functions for MMIO access
    uint32_t mmio_read32(uint64_t addr)
    {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        uint32_t data;

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr((unsigned char *)&data);
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        bar0_socket->b_transport(trans, delay);
        wait(delay);

        return data;
    }

    void mmio_write32(uint64_t addr, uint32_t data)
    {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;

        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr((unsigned char *)&data);
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        bar0_socket->b_transport(trans, delay);
        wait(delay);
    }

    void mmio_write64(uint64_t addr, uint64_t data)
    {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;

        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr((unsigned char *)&data);
        trans.set_data_length(8);
        trans.set_streaming_width(8);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        bar0_socket->b_transport(trans, delay);
        wait(delay);
    }
};

int sc_main(int argc, char *argv[])
{
    // Instantiate components
    matrix_multiplier_pcie device("matrix_device");
    host_memory memory("host_memory");
    test_driver driver("test_driver");

    // Signals
    sc_signal<bool> irq;

    // Connections
    driver.bar0_socket.bind(device.bar0_target_socket);
    device.dma_initiator_socket.bind(memory.target_socket);
    device.interrupt(irq);
    driver.interrupt(irq);

    // Setup test data in host memory
    // Matrix A (4x4) = identity matrix
    float matrix_a[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1};

    // Matrix B (4x4) = [2, 2, 2, 2, ...]
    float matrix_b[16];
    for (int i = 0; i < 16; i++)
        matrix_b[i] = 2.0f;

    memory.write_data(0x1000, matrix_a, sizeof(matrix_a));
    memory.write_data(0x2000, matrix_b, sizeof(matrix_b));

    cout << "==================================================" << endl;
    cout << "Starting SystemC Simulation" << endl;
    cout << "==================================================" << endl;

    // Run simulation
    sc_start();

    // Read result
    float matrix_c[16];
    memory.read_data(0x3000, matrix_c, sizeof(matrix_c));

    cout << "\n==================================================" << endl;
    cout << "Result Matrix C:" << endl;
    for (int i = 0; i < 4; i++)
    {
        cout << "  ";
        for (int j = 0; j < 4; j++)
        {
            cout << matrix_c[i * 4 + j] << " ";
        }
        cout << endl;
    }
    cout << "==================================================" << endl;

    return 0;
}