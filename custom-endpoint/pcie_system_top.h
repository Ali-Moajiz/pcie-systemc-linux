#ifndef PCIE_SYSTEM_TOP_H
#define PCIE_SYSTEM_TOP_H

/*                  ---> T H E  F L O W  <---



 PCIE Transaction                        TLM initiator sockets
      Layer           .----------------.      (BAR0-BAR5)
 [TLP packet side]    |                |--------------------->
                      |                |--------------------->
                      |                |--------------------->  [User logic side]
   TLM target socket  | PCIeController |--------------------->
   ------------------>|                |--------------------->
                      |                |--------------------->
                      |                |
 TLM initiator socket |                |  TLM target socket
   <------------------|                | (DMA to the PCIE interface)
                      |                |<---------------------
                      |                |
                      |                | MSI-X interrupts
                      |                | (sc_signal vector)
                      |                |<---------------------
                      '----------------'


*/

#include <systemc>
#include <tlm>
#include "matrix_multiplier_pcie.h"
#include "pf-config.h"
// Include Xilinx PCIe controller headers
// #include "pcie/xilinx/pcie_ep_device.h"
// #include "pcie/xilinx/pcie_controller.h"

using namespace sc_core;

SC_MODULE(pcie_system_top)
{
public:
    // Your matrix multiplier endpoint
    matrix_multiplier_pcie *matrix_device;
    PCIeController *pcie_controller;

    // Xilinx PCIe controller (you'll instantiate this)
    // xilinx_pcie_controller *pcie_ctrl;

    // Interrupt signal
    sc_signal<bool> irq_signal;

    SC_CTOR(pcie_system_top)
    {
        PhysFuncConfig pf_cfg; // Need to initiallize at once, Going with the default values

        matrix_device = new matrix_multiplier_pcie("matrix_device");
        pcie_controller = new PCIeController("xilinx pcie controller", pf_cfg, true);

        // Connect interrupt
        matrix_device->interrupt(pcie_controller->signals_irq[0]);

        // Connect BAR0 to PCIe controller's target socket
        pcie_controller->bar0_init_socket.bind(matrix_device->bar0_target_socket);
        // Connect DMA initiator from device to PCIe controller's target socket
        matrix_device->dma_initiator_socket.bind(pcie_controller->dma_tgt_socket);

        pcie_controller->cout << "==================================================" << endl;
        cout << "Matrix Multiplier PCIe Device Initialized" << endl;
        cout << "==================================================" << endl;
        cout << "Register Map (BAR0):" << endl;
        cout << "  0x0000 - CONTROL      (R/W)" << endl;
        cout << "  0x0004 - STATUS       (R)" << endl;
        cout << "  0x0008 - DIM_N        (R/W)" << endl;
        cout << "  0x0010 - MATRIX_A_PTR (R/W, 64-bit)" << endl;
        cout << "  0x0018 - MATRIX_B_PTR (R/W, 64-bit)" << endl;
        cout << "  0x0020 - MATRIX_C_PTR (R/W, 64-bit)" << endl;
        cout << "  0x0028 - INT_STATUS   (R/W1C)" << endl;
        cout << "  0x002C - INT_ENABLE   (R/W)" << endl;
        cout << "==================================================" << endl;
    }

    ~pcie_system_top()
    {
        delete matrix_device;
        delete pcie_controller;
    }
};

#endif // PCIE_SYSTEM_TOP_H