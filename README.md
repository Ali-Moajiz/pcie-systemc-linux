# PCIe SystemC-QEMU Co-Simulation Project (WIP, Learning different implementation techniques)

## Overview

This project implements a co-simulation environment integrating QEMU with a SystemC-based PCIe endpoint. The system demonstrates hardware-software co-design for PCIe-based computation, specifically implementing a 4x4 matrix multiplication accelerator.

The project is inspired by Xilinx's co-simulation platform, see [Co-simulation](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/862421112/Co-simulation)

### Architecture

The project consists of three main components:

1. **QEMU Virtual Machine**: Runs a Linux guest OS with a custom PCIe proxy device that routes transactions between the kernel driver and the SystemC model
2. **SystemC PCIe Endpoint**: Implements the Xilinx PCIe Controller (transaction layer) as the endpoint device with matrix multiplication hardware logic
3. **Linux Kernel Driver**: Facilitates communication between user-space applications and the PCIe endpoint
4. **Remote-Port Protocol**: Remote-port protocol is Xillinx's developed protocol for co-simulation between two different simulation platforms. [Remote-port protocol architecture](https://github.com/Xilinx/libsystemctlm-soc/blob/master/docs/remote-port-proto.md)

### Data Flow

```
User Application → Kernel Driver → QEMU Proxy Device → Unix Socket → SystemC Endpoint (Matrix Multiplier) → Results back to User
```

The user-space application sends two 4x4 matrices to the kernel driver, which communicates with the QEMU proxy PCIe device. This proxy routes the data through a Unix socket to the SystemC model containing the Xilinx PCIe controller and matrix multiplication logic. The computed result flows back through the same path to the user application.

### Xilinx PCIe Controller

The SystemC model uses the Xilinx PCIe Controller, which implements the transaction layer of the PCIe protocol stack. This controller handles TLP (Transaction Layer Packet) generation, flow control, and PCIe-compliant communication.

For more details about the Xilinx PCIe model, visit: https://github.com/Xilinx/pcie-model

## Prerequisites

- SystemC simulation environment (SystemC 2.3 or later)
- QEMU with custom PCIe device support
- Buildroot-generated Linux images (kernel + rootfs)
- Python 3 (for socket dump tool)
- GCC/G++ compiler with C++11 support

## Running the Co-Simulation

### Important: Terminal Setup

Since this is a **co-simulation project**, SystemC is the UNIX socket server and QEMU is the client; you need to launch components in **two separate terminals** (or three if using the monitoring tool):

- **Terminal 1**: First launch SystemC PCIe model (the SystemC model will be waiting for the client to connect in the `accept()` API of the socket connection)
- **Terminal 2**: QEMU virtual machine (QEMU process will connect to the server as client)
- **Terminal 3** (Optional): Socket monitoring tool (in the third terminal to monitor the packet transfer between the two processes)

### Step 1: Start the SystemC Model (Terminal 1)

First, launch the PCIe SystemC simulation model:

```bash
./pcie_sim unix:/tmp/qemu-rport 10000
```

This starts the SystemC model with:
- Unix socket interface at `/tmp/qemu-rport`
- Port number 10000
- Xilinx PCIe controller ready to accept transactions

**Keep this terminal running** throughout the simulation.

### Step 2: Launch QEMU (Terminal 2)

In a **separate terminal**, run the QEMU emulator:

```bash
./qemu-system-x86_64 -M q35 -m 2G \
  -kernel ../../../buildroot/output/images/bzImage \
  -drive file=../../../buildroot/output/images/rootfs.ext2,if=virtio,format=raw \
  -append "root=/dev/vda console=ttyS0 rw" \
  -nographic \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -fsdev local,id=fsdev0,path=/home/moajiz/pcie-systemc-linux/kernel-device-driver,security_model=none \
  -device virtio-9p-pci,fsdev=fsdev0,mount_tag=hostshare \
  -device pcie-root-port,id=rootport0,chassis=1,slot=10 \
  -chardev socket,id=rp_socket,path=/tmp/qemu-rport,server=off \
  -device pcie-mm,rp-chardev=rp_socket,bus=rootport0,addr=0x0
```

**Key QEMU Parameters:**
- `-device pcie-mm,rp-chardev=rp_socket`: Custom PCIe proxy device connected to SystemC via socket
- `-device pcie-root-port`: PCIe root port for device attachment
- `-fsdev` and `-device virtio-9p-pci`: Shared folder for driver development

**Note:** Update the `path` parameter in the `-fsdev` option to match your actual kernel device driver directory location.

### Step 3: Mount the Shared Folder (Inside QEMU Guest)

Once the Linux system boots inside QEMU, mount the shared folder to access the kernel driver:

```bash
mkdir -p /mnt/hostshare && mount -t 9p -o trans=virtio,version=9p2000.L hostshare /mnt/hostshare
```

This makes the kernel device driver directory accessible at `/mnt/hostshare` inside the guest.

### Step 4: Monitor Socket Communication (Optional - Terminal 3)

To monitor the PCIe transaction communication between QEMU and SystemC, run the socket dump tool in a **third terminal**:

```bash
cd dump-tool
sudo python3 sockdump.py "/tmp/qemu-rport"
```

This tool captures and displays TLPs and other data exchanged over the Unix socket in real-time.

## Project Structure

```
.
├── pcie_sim                  # SystemC PCIe endpoint executable (Xilinx controller + matrix multiplier)
├── qemu-system-x86_64        # QEMU emulator with custom PCIe proxy device
├── dump-tool/                # Socket monitoring and debugging tools
│   └── sockdump.py           # Python script to dump PCIe transactions
├── kernel-device-driver/     # Linux kernel driver for PCIe communication
└── buildroot/
    └── output/images/        # Linux kernel and rootfs images
        ├── bzImage
        └── rootfs.ext2
```

## How It Works

### Matrix Multiplication Flow

1. **User Application** prepares two 4x4 matrices and sends them to the kernel driver via ioctl/read/write calls
2. **Kernel Driver** formats the matrices into PCIe memory/IO transactions
3. **QEMU Proxy Device** (`pcie-mm`) intercepts these transactions and forwards them through the Unix socket
4. **SystemC Endpoint** receives the TLPs via the Xilinx PCIe controller, extracts matrix data, performs multiplication in hardware logic
5. **Result Return Path**: Computed matrix flows back through the same path (SystemC → Socket → QEMU → Driver → User)

### PCIe Transaction Layer

The Xilinx PCIe controller in SystemC handles:
- TLP packet generation and parsing
- Memory Read/Write requests
- Completion packets with data
- Flow control and credit management
- PCIe protocol compliance

## Development Workflow

1. Modify the matrix multiplication logic in the SystemC model
2. Rebuild the `pcie_sim` executable
3. Update the kernel driver if interface changes are needed
4. Test by running the co-simulation and observing results

## References

- [Xilinx PCIe Model Repository](https://github.com/Xilinx/pcie-model)
- SystemC Documentation
- QEMU PCIe Device Documentation
- Linux Kernel Driver Development Guide

The references will be added...
