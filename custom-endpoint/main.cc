/*
 * SystemC PCIe Co-simulation Model
 *
 * Architecture:
 *   QEMU (Remote Port) <--> PCIe Adapter <--> Xilinx PCIe Controller <--> Matrix Multiplier
 */

// Define this BEFORE including systemc to allow deprecated API
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <iomanip>
#include <iostream>

#include <unistd.h> // For unlink()
#include <cstdlib>  // For atoi()
#include <cstring>  // For strncmp()

// Add namespace to fix library header issues
using namespace sc_core;
using namespace sc_dt;
using namespace std;

// Xilinx Remote Port includes
#include "remote-port-tlm.h"
#include "remote-port-tlm-memory-master.h"
#include "remote-port-tlm-memory-slave.h"

// Xilinx PCIe Controller
#include "pcie-controller.h"

/* ============================================================================
 * PCIe Remote Port Adapter
//  * ============================================================================ */
// SC_MODULE(PCIeRPAdapter)
// {
//     remoteport_tlm *rp_adapter;
//     tlm_utils::simple_initiator_socket<PCIeRPAdapter> rp_init_socket;
//     tlm_utils::simple_target_socket<PCIeRPAdapter> rp_tgt_socket;

//     // Add reset signal
//     sc_signal<bool> rst_signal;

//     uint64_t bar_base_address;
//     uint64_t bar_size;

//     SC_HAS_PROCESS(PCIeRPAdapter);

//     PCIeRPAdapter(sc_module_name name, int rp_port, uint64_t bar_addr, uint64_t bar_sz)
//         : sc_module(name),
//           rp_init_socket("rp_init_socket"),
//           rp_tgt_socket("rp_tgt_socket"),
//           rst_signal("rst_signal"),
//           bar_base_address(bar_addr),
//           bar_size(bar_sz)
//     {
//         // Initialize reset signal to false (not in reset)
//         rst_signal.write(false);

//         // Create remote port adapter with proper parameters
//         rp_adapter = new remoteport_tlm("rp_adapter", rp_port, nullptr, nullptr, false);

//         // Bind the reset signal to the remote port adapter
//         rp_adapter->rst(rst_signal);

//         // Register target socket callback
//         rp_tgt_socket.register_b_transport(this, &PCIeRPAdapter::b_transport_from_pcie);

//         cout << "PCIeRPAdapter initialized. Remote Port: localhost:" << rp_port << endl;
//         cout << "BAR Address: 0x" << hex << bar_base_address << ", Size: 0x" << bar_size << dec << endl;
//     }

//     ~PCIeRPAdapter()
//     {
//         delete rp_adapter;
//     }

//     void b_transport_from_qemu(tlm::tlm_generic_payload & trans, sc_time & delay)
//     {
//         tlm::tlm_command cmd = trans.get_command();
//         uint64_t addr = trans.get_address();
//         unsigned char *data = trans.get_data_ptr();
//         unsigned int len = trans.get_data_length();

//         if (addr < bar_base_address || addr >= bar_base_address + bar_size)
//         {
//             trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
//             return;
//         }

//         uint64_t bar_offset = addr - bar_base_address;

//         tlm::tlm_generic_payload pcie_trans;
//         pcie_trans.set_command(cmd);
//         pcie_trans.set_address(bar_offset);
//         pcie_trans.set_data_ptr(data);
//         pcie_trans.set_data_length(len);
//         pcie_trans.set_streaming_width(len);
//         pcie_trans.set_byte_enable_ptr(nullptr);
//         pcie_trans.set_dmi_allowed(false);
//         pcie_trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

//         sc_time pcie_delay = delay;
//         rp_init_socket->b_transport(pcie_trans, pcie_delay);

//         trans.set_response_status(pcie_trans.get_response_status());
//         delay = pcie_delay;
//     }

//     void b_transport_from_pcie(tlm::tlm_generic_payload & trans, sc_time & delay)
//     {
//         trans.set_response_status(tlm::TLM_OK_RESPONSE);
//     }
// };
// /* ============================================================================
//  * Matrix Multiplier
//  * ============================================================================ */
// SC_MODULE(MatrixMultiplier)
// {
//     tlm_utils::simple_target_socket<MatrixMultiplier> bar0_tgt_socket;

//     uint32_t op1[4][4];
//     uint32_t op2[4][4];
//     uint32_t opcode;
//     uint32_t result[4][4];
//     bool result_valid;

//     SC_HAS_PROCESS(MatrixMultiplier);

//     MatrixMultiplier(sc_module_name name)
//         : sc_module(name), bar0_tgt_socket("bar0_tgt_socket"), opcode(0), result_valid(false)
//     {
//         bar0_tgt_socket.register_b_transport(this, &MatrixMultiplier::b_transport_bar0);
//         for (int i = 0; i < 4; i++)
//             for (int j = 0; j < 4; j++)
//                 op1[i][j] = op2[i][j] = result[i][j] = 0;
//     }

//     void b_transport_bar0(tlm::tlm_generic_payload & trans, sc_time & delay)
//     {
//         tlm::tlm_command cmd = trans.get_command();
//         uint64_t addr = trans.get_address();
//         uint32_t *data32 = reinterpret_cast<uint32_t *>(trans.get_data_ptr());

//         if (trans.get_data_length() != 4)
//         {
//             trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
//             return;
//         }

//         if (cmd == tlm::TLM_WRITE_COMMAND)
//             handle_write(addr, *data32);
//         else
//             *data32 = handle_read(addr);

//         trans.set_response_status(tlm::TLM_OK_RESPONSE);
//         delay += sc_time(10, SC_NS);
//     }

//     void handle_write(uint64_t addr, uint32_t value)
//     {
//         if (addr >= 0x10 && addr < 0x50)
//         {
//             int index = (addr - 0x10) / 4;
//             op1[index / 4][index % 4] = value;
//             result_valid = false;
//         }
//         else if (addr >= 0x50 && addr < 0x90)
//         {
//             int index = (addr - 0x50) / 4;
//             op2[index / 4][index % 4] = value;
//             result_valid = false;
//         }
//         else if (addr == 0x90)
//         {
//             opcode = value;
//             result_valid = false;
//         }
//     }

//     uint32_t handle_read(uint64_t addr)
//     {
//         if (addr >= 0xA0 && addr < 0xE0)
//         {
//             if (addr == 0xA0 && !result_valid)
//                 perform_multiplication();
//             int index = (addr - 0xA0) / 4;
//             return result[index / 4][index % 4];
//         }
//         else if (addr == 0x90)
//         {
//             return opcode;
//         }
//         return 0xFFFFFFFF;
//     }

//     void perform_multiplication()
//     {
//         if (opcode != 1)
//         {
//             for (int i = 0; i < 4; i++)
//                 for (int j = 0; j < 4; j++)
//                     result[i][j] = 0xFFFFFFFF;
//         }
//         else
//         {
//             for (int i = 0; i < 4; i++)
//                 for (int j = 0; j < 4; j++)
//                 {
//                     result[i][j] = 0;
//                     for (int k = 0; k < 4; k++)
//                         result[i][j] += op1[i][k] * op2[k][j];
//                 }
//         }
//         result_valid = true;
//     }
// };

// /* ============================================================================
//  * Top-Level Module
//  * ============================================================================ */
// SC_MODULE(PCIeCoSimTop)
// {
//     PCIeRPAdapter *rp_adapter;
//     PCIeController *pcie_controller;
//     MatrixMultiplier *matrix_mult;

//     SC_HAS_PROCESS(PCIeCoSimTop);

//     PCIeCoSimTop(sc_module_name name, int rp_port = 9000)
//         : sc_module(name)
//     {
//         PhysFuncConfig cfg;

//         // Configure basic PCIe device identity
//         cfg.SetPCIVendorID(0x10EE);
//         cfg.SetPCIDeviceID(0x0001);
//         cfg.SetPCIRevisionID(0x01);
//         cfg.SetPCIClassBase(0x05);
//         cfg.SetPCIClassProgIF(0x00);

//         // Configure BAR0 for your MatrixMultiplier device (1MB)
//         cfg.SetPCIBAR0(0x100000, PCI_BASE_ADDRESS_SPACE_MEMORY);

//         // Configure BAR1 for MSI-X (4KB is sufficient)
//         cfg.SetPCIBAR1(0x1000, PCI_BASE_ADDRESS_SPACE_MEMORY);

//         // Create and configure MSI-X capability
//         MSIXCapability msix_cap;

//         // Set table to use BAR1 (BIR = BAR Index Register)
//         // TableOffsetBIR format: lower 3 bits = BIR, upper bits = offset
//         msix_cap.SetTableOffsetBIR(1); // BIR = 1 means use BAR1, offset = 0

//         // Set Pending Bit Array to use BAR1 with offset
//         msix_cap.SetPendingBitArray(0x800 | 1); // BIR = 1, offset = 0x800 (2KB into BAR1)

//         // Set number of MSI-X vectors (e.g., 4 vectors)
//         // MessageControl register format: bits 10:0 = Table Size - 1
//         msix_cap.SetMessageControl(3); // 3 means 4 vectors (0-indexed)

//         // Add MSI-X capability to configuration
//         cfg.AddPCICapability(msix_cap);
//         // NOte: Need to see the MSI-X stuff when implementing this. At this point in time, just initializing some required stuff here.
//         rp_adapter = new PCIeRPAdapter("rp_adapter", rp_port, 0x0, 0x100000);
//         pcie_controller = new PCIeController("PCIe_Controller", cfg, false);
//         matrix_mult = new MatrixMultiplier("matrix_mult");

//         // Bind sockets
//         rp_adapter->rp_init_socket.bind(pcie_controller->tgt_socket);
//         pcie_controller->init_socket.bind(rp_adapter->rp_tgt_socket);
//         pcie_controller->bar0_init_socket.bind(matrix_mult->bar0_tgt_socket);

//         cout << "PCIe Device Configuration:" << endl;
//         cout << "  Vendor ID: 0x" << hex << cfg.GetPCIVendorID() << endl;
//         cout << "  Device ID: 0x" << cfg.GetPCIDeviceID() << endl;
//         cout << "  BAR0: Device registers (1MB)" << endl;
//         cout << "  BAR1: MSI-X capability (4KB)" << dec << endl;
//     }

//     ~PCIeCoSimTop()
//     {
//         delete rp_adapter;
//         delete pcie_controller;
//         delete matrix_mult;
//     }
// };

/* ============================================================================
 * main
 * ============================================================================ */
// int sc_main(int argc, char *argv[])
// {
//     int rp_port = 9000;
//     if (argc > 1)
//         rp_port = atoi(argv[1]);

//     PCIeCoSimTop top("top", rp_port);
//     sc_start();
//     return 0;
// }

#include <sys/types.h>
// Helper to check if the socket file exists on disk
// Minimal Dummy Target to acknowledge the handshake
// Minimal Dummy Target to acknowledge the handshake
SC_MODULE(DummyTarget)
{
    tlm_utils::simple_target_socket<DummyTarget> socket;
    SC_CTOR(DummyTarget) : socket("socket")
    {
        socket.register_b_transport(this, &DummyTarget::b_transport);
    }
    void b_transport(tlm::tlm_generic_payload & trans, sc_time & delay)
    {
        printf("SystemC: Received TLM transaction!\n");
        uint64_t addr = trans.get_address();

        if (trans.is_read())
        {
            uint32_t data = 0xABCD;
            memcpy(trans.get_data_ptr(), &data, 5);
            printf("SystemC: Received READ at 0x%lx, returning 0x%x\n", addr, data);
        }
        else
        {
            printf("SystemC: Received WRITE at 0x%lx\n", addr);
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

int sc_main(int argc, char *argv[])
{
    const char *sk_descr = (argc > 1) ? argv[1] : "unix:/tmp/qemu-rport";
    const char *socket_file = (strncmp(sk_descr, "unix:", 5) == 0) ? sk_descr + 5 : sk_descr;

    printf("=== SystemC RemotePort Server ===\n");

    if (access(socket_file, F_OK) == 0)
    {
        printf("Cleaning old socket: %s\n", socket_file);
        unlink(socket_file);
    }

    sc_signal<bool> rst("rst");

    remoteport_tlm *rp_adapter = new remoteport_tlm("rp_adapter", -1, sk_descr, NULL, true);
    rp_adapter->rst(rst);

    remoteport_tlm_memory_master *rp_master = new remoteport_tlm_memory_master("rp_master");
    rp_adapter->register_dev(0, rp_master);

    DummyTarget *dummy = new DummyTarget("dummy");
    rp_master->sk.bind(dummy->socket);

    rst.write(true);
    sc_start(SC_ZERO_TIME);

    printf("Listening for QEMU on: %s\n", sk_descr);
    rst.write(false);
    
    // **ADD THIS**: Give SystemC time to fully set up the socket listener
    sc_start(100, SC_MS);  // Wait 100ms for socket to be ready
    
    printf("Socket ready. Waiting for QEMU connection...\n");
    
    // Now start main simulation
    sc_start();
    return 0;
}
