#ifndef PCIE_SYSTEM_TOP_H
#define PCIE_SYSTEM_TOP_H

/*                  ---> T H E  F L O W  <---

                                              
 QEMU Process              SystemC Process
 ┌─────────────┐          ┌──────────────────────────────────┐
 │Root Complex │  Socket  │ PCIeQemuBridge                   │
 │ └─Root Port │◄────────►│  ├─remoteport_tlm_pci_ep         │
 │             │   IPC    │  └─pcie_root_port (TLM adapter)  │
 └─────────────┘          │            ↕                     │
                          │      PCIeController               │
                          │       ├─ BAR0-5 (init sockets)    │
                          │       ├─ DMA (target socket)      │
                          │       └─ MSI-X (signals)          │
                          │            ↕                     │
                          │   matrix_multiplier_pcie         │
                          │    (Your Endpoint Device)        │
                          └──────────────────────────────────┘
*/

#include <systemc>
#include <tlm>
#include "matrix_multiplier_pcie.h"
#include "pci-defs-fix.h"  // Add PCI definitions before pf-config.h
#include "pcie_api.h"
#include "pf-config.h"
#include "qemu-bridge.h"
#include "pcie-controller.h"

using namespace sc_core;

SC_MODULE(pcie_system_top)
{
public:
    // Components
    matrix_multiplier_pcie *matrix_device;
    PCIeController *pcie_controller;
    PCIeQemuBridge *qemu_bridge;

    // Reset signal (must be toggled in sc_main)
    sc_signal<bool> rst;

    // Interrupt signals (if using MSI-X)
    sc_vector<sc_signal<bool>> irq_signals;

    SC_CTOR(pcie_system_top) :
        irq_signals("irq_signals", 1)  // Assuming 1 interrupt for now
    {
        // ============================================
        // 1. Create Physical Function Configuration
        // ============================================
        PhysFuncConfig pf_cfg; //create_pf_config();

        // ============================================
        // 2. Instantiate Components
        // ============================================
        
        // Your matrix multiplier PCIe endpoint device
        matrix_device = new matrix_multiplier_pcie("matrix_device");
        
        // PCIe Controller (manages BARs, DMA, MSI-X)
        pcie_controller = new PCIeController("pcie_controller", pf_cfg);
        
        // QEMU Bridge (connects to QEMU via Remote-Port socket)
        // The socket path should match QEMU's -chardev socket parameter
        const char* socket_path = "unix:/tmp/qemu-rp-0";
        qemu_bridge = new PCIeQemuBridge("qemu_bridge", socket_path);

        // ============================================
        // 3. Connect Reset Signal
        // ============================================
        qemu_bridge->rst(rst);

        // ============================================
        // 4. Connect QEMU Bridge <-> PCIe Controller
        // ============================================
        // This connects the TLP packet flow between QEMU and the controller
        qemu_bridge->rootport.init_socket.bind(pcie_controller->tgt_socket);
        pcie_controller->init_socket.bind(qemu_bridge->rootport.tgt_socket);

        // ============================================
        // 5. Connect PCIe Controller <-> Device
        // ============================================
        
        // Connect BAR0 (register interface)
        pcie_controller->bar0_init_socket.bind(matrix_device->bar0_target_socket);
        
        // Connect DMA path (device -> host memory via QEMU)
        matrix_device->dma_initiator_socket.bind(pcie_controller->dma_tgt_socket);
        
        // Connect interrupts (device -> controller)
        // Note: irq is private, need to use public method or signals
        // For now, connect device interrupt directly to irq_signals
        matrix_device->interrupt(irq_signals[0]);
        // The controller will read from irq_signals through its internal connections

        // ============================================
        // Print Configuration Info
        // ============================================
        print_device_info();
    }

    ~pcie_system_top()
    {
        delete matrix_device;
        delete pcie_controller;
        delete qemu_bridge;
    }

private:
    /**
     * Create Physical Function Configuration
     * This defines the PCIe device's capabilities, BARs, etc.
     */
    // PhysFuncConfig create_pf_config()
    // {
    //     PhysFuncConfig cfg;
    //     PMCapability pmCap;
    //     PCIExpressCapability pcieCap;
    //     MSIXCapability msixCap;

    //     // Vendor/Device IDs (customize these for your device)
    //     cfg.SetPCIVendorID(0x10EE);  // Xilinx vendor ID
    //     cfg.SetPCIDeviceID(0x9038);  // Custom device ID
    //     cfg.SetPCISubsystemVendorID(0x10EE);
    //     cfg.SetPCISubsystemID(0x0001);

    //     // Device class (0x12 = Processing Accelerator)
    //     cfg.SetPCIClassBase(0x12);
    //     cfg.SetPCIClassDevice(0x00);
    //     cfg.SetPCIClassProgIF(0x00);

    //     // BAR Configuration
    //     // BAR0: 4KB for registers (64-bit BAR)
    //     uint32_t bar_flags = PCI_BASE_ADDRESS_MEM_TYPE_64;
    //     cfg.SetPCIBAR0(4 * 1024, bar_flags);
        
    //     // BAR2-5: Optional, add if you need more BARs
    //     // cfg.SetPCIBAR2(64 * 1024, bar_flags);

    //     // Expansion ROM (disabled)
    //     cfg.SetPCIExpansionROMBAR(0, 0);

    //     // Add PM Capability
    //     cfg.AddPCICapability(pmCap);

    //     // Add PCIe Capability
    //     uint32_t maxLinkWidth = 1 << 4;  // x1 link width
    //     pcieCap.SetDeviceCapabilities(PCI_EXP_DEVCAP_RBER);
    //     pcieCap.SetLinkCapabilities(PCI_EXP_LNKCAP_SLS_2_5GB | maxLinkWidth);
    //     pcieCap.SetLinkStatus(PCI_EXP_LNKSTA_CLS_2_5GB | PCI_EXP_LNKSTA_NLW_X1);
    //     cfg.AddPCICapability(pcieCap);

    //     // Add MSI-X Capability
    //     uint32_t msixTableSz = 1;  // Number of interrupt vectors
    //     uint32_t tableOffset = 0x1000 | 0;  // Offset 0x1000, BIR 0 (BAR0)
    //     uint32_t pba = 0x2000 | 0;  // PBA at offset 0x2000, BIR 0
        
    //     msixCap.SetMessageControl(msixTableSz - 1);
    //     msixCap.SetTableOffsetBIR(tableOffset);
    //     msixCap.SetPendingBitArray(pba);
    //     cfg.AddPCICapability(msixCap);

    //     return cfg;
    // }

    void print_device_info()
    {
        std::cout << "==================================================" << std::endl;
        std::cout << "Matrix Multiplier PCIe Device Initialized" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "Register Map (BAR0):" << std::endl;
        std::cout << "  0x0000 - CONTROL      (R/W)" << std::endl;
        std::cout << "  0x0004 - STATUS       (R)" << std::endl;
        std::cout << "  0x0008 - DIM_N        (R/W)" << std::endl;
        std::cout << "  0x0010 - MATRIX_A_PTR (R/W, 64-bit)" << std::endl;
        std::cout << "  0x0018 - MATRIX_B_PTR (R/W, 64-bit)" << std::endl;
        std::cout << "  0x0020 - MATRIX_C_PTR (R/W, 64-bit)" << std::endl;
        std::cout << "  0x0028 - INT_STATUS   (R/W1C)" << std::endl;
        std::cout << "  0x002C - INT_ENABLE   (R/W)" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "QEMU Connection: Ready for Remote-Port socket" << std::endl;
        std::cout << "Socket Path: /tmp/qemu-rp-0" << std::endl;
        std::cout << "==================================================" << std::endl;
    }
};

#endif // PCIE_SYSTEM_TOP_H