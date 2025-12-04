#ifndef SYSTEMC_TLM_QEMU_BRIDGE_PCIE_H
#define SYSTEMC_TLM_QEMU_BRIDGE_PCIE_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

// Include remote-port components from libsystemctlm-soc
#include "remote-port-tlm.h"
#include "remote-port-tlm-pci-ep.h"
#include "soc/pci/core/pcie-root-port.h"

using namespace sc_core;

/**
 * PCIeQemuBridge - Bridge between QEMU PCIe Root Complex and SystemC PCIe Endpoint
 *
 * Architecture:
 *   QEMU (Root Complex) <--> [Unix Socket] <--> remoteport_tlm_pci_ep <-->
 *   pcie_root_port (TLM adapter) <--> PCIeController <--> Your Device
 *
 * This module encapsulates:
 *   1. remoteport_tlm_pci_ep: Handles socket communication with QEMU
 *   2. pcie_root_port: Adapts Remote-Port protocol to TLM transactions
 */
SC_MODULE(PCIeQemuBridge)
{
public:
    // TLM sockets - connect to PCIeController
    // These are bound to the pcie_root_port's sockets internally
    tlm_utils::simple_initiator_socket<PCIeQemuBridge> init_socket;
    tlm_utils::simple_target_socket<PCIeQemuBridge> tgt_socket;

    // Reset signal
    sc_in<bool> rst;

    // Public access to internal components (needed for socket binding)
    remoteport_tlm_pci_ep rp_pci_ep;
    pcie_root_port rootport;

    SC_HAS_PROCESS(PCIeQemuBridge);

    /**
     * Constructor
     * @param name Module name
     * @param sk_descr Socket descriptor string for Remote-Port connection
     *                 Format: "unix:path/to/socket" or "tcp:hostname:port"
     *                 Example: "unix:/tmp/qemu-rp-0"
     */
    PCIeQemuBridge(sc_module_name name, const char *sk_descr) : sc_module(name),
                                                                init_socket("init_socket"),
                                                                tgt_socket("tgt_socket"),
                                                                rst("rst"),
                                                                rp_pci_ep("rp-pci-ep",
                                                                          0,         // Adapters (not used for basic setup)
                                                                          1,         // Number of devs
                                                                          0,         // Offset
                                                                          sk_descr), // Socket descriptor
                                                                rootport("rootport")
    {
        // Connect reset signal to remote-port
        rp_pci_ep.rst(rst);

        // Bind remote-port to root port adapter
        rp_pci_ep.bind(rootport);

        // Register TLM transport callbacks
        tgt_socket.register_b_transport(this, &PCIeQemuBridge::b_transport);
    }

private:
    /**
     * Handle incoming TLM transactions from PCIeController (device -> QEMU)
     * This handles DMA requests from the device going upstream to host memory
     */
    void b_transport(tlm::tlm_generic_payload & trans, sc_time & delay)
    {
        // Forward DMA transactions to QEMU through the rootport
        // The rootport init_socket is already connected to QEMU via rp_pci_ep
        // So we forward to init_socket which will route to QEMU
        init_socket->b_transport(trans, delay);
    }
};

#endif // SYSTEMC_TLM_QEMU_BRIDGE_PCIE_H