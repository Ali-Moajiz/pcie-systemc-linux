#ifndef PCIE_SYSTEM_TOP_H
#define PCIE_SYSTEM_TOP_H

#include <systemc>
#include <tlm>
#include "matrix_multiplier_pcie.h"
// Include Xilinx PCIe controller headers
// #include "pcie/xilinx/pcie_ep_device.h"
// #include "pcie/xilinx/pcie_controller.h"

using namespace sc_core;

SC_MODULE(pcie_system_top)
{
public:
    // Your matrix multiplier endpoint
    matrix_multiplier_pcie *matrix_device;

    // Xilinx PCIe controller (you'll instantiate this)
    // xilinx_pcie_controller *pcie_ctrl;

    // Interrupt signal
    sc_signal<bool> irq_signal;

    SC_CTOR(pcie_system_top)
    {
        // Instantiate the matrix multiplier device
        matrix_device = new matrix_multiplier_pcie("matrix_device");

        // Connect interrupt
        matrix_device->interrupt(irq_signal);

        // TODO: Instantiate Xilinx PCIe controller
        // pcie_ctrl = new xilinx_pcie_controller("pcie_ctrl");

        // Connect BAR0 to PCIe controller's target socket
        // This is how the PCIe controller will forward MMIO transactions to your device
        // pcie_ctrl->bar0_initiator_socket.bind(matrix_device->bar0_target_socket);

        // Connect DMA initiator from device to PCIe controller's target socket
        // This is how your device performs DMA to host memory
        // matrix_device->dma_initiator_socket.bind(pcie_ctrl->dma_target_socket);

        // Connect interrupt to PCIe controller
        // pcie_ctrl->interrupt_in(irq_signal);

        cout << "==================================================" << endl;
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
        // delete pcie_ctrl;
    }
};

#endif // PCIE_SYSTEM_TOP_H