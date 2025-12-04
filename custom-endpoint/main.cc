#include <systemc>
#include <signal.h>
#include <unistd.h>
#include "pcie_system_top.h"
#include "tlm_utils/tlm_quantumkeeper.h"

#include <systemc.h>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include "matrix_multiplier_pcie.h"

using namespace sc_core;

// Global pointer for signal handler
static pcie_system_top *g_top = nullptr;

// Signal handler for clean shutdown (Ctrl+C)
void sigint_handler(int sig)
{
    std::cout << "\n[sc_main] Caught signal " << sig << ", stopping simulation..." << std::endl;
    sc_stop();
}

void usage(const char *prog_name)
{
    std::cout << "Usage: " << prog_name << " <socket-path> [sync-quantum-ns]" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  socket-path      : Unix socket path for Remote-Port connection" << std::endl;
    std::cout << "                     (must match QEMU's -chardev socket parameter)" << std::endl;
    std::cout << "                     Example: unix:/tmp/qemu-rp-0" << std::endl;
    std::cout << "  sync-quantum-ns  : Synchronization quantum in nanoseconds (default: 10000)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << prog_name << " unix:/tmp/qemu-rp-0 10000" << std::endl;
}

int sc_main(int argc, char *argv[])
{
    // ============================================
    // Parse Command Line Arguments
    // ============================================
    if (argc < 2)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *socket_path = argv[1];
    uint64_t sync_quantum_ns = (argc >= 3) ? strtoull(argv[2], nullptr, 10) : 10000;

    std::cout << "==================================================" << std::endl;
    std::cout << "SystemC PCIe Endpoint Simulation" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Socket Path: " << socket_path << std::endl;
    std::cout << "Sync Quantum: " << sync_quantum_ns << " ns" << std::endl;
    std::cout << "==================================================" << std::endl;

    // ============================================
    // Setup SystemC Environment
    // ============================================

    // Set time resolution
    sc_set_time_resolution(1, SC_PS);

    // Setup quantum keeper for TLM temporal decoupling
    tlm_utils::tlm_quantumkeeper qk;
    qk.set_global_quantum(sc_time(sync_quantum_ns, SC_NS));

    // ============================================
    // Instantiate Top-Level Module
    // ============================================
    g_top = new pcie_system_top("top");

    // ============================================
    // Setup Signal Handler
    // ============================================
    signal(SIGINT, sigint_handler);

    // ============================================
    // Optional: Setup VCD Tracing
    // ============================================
    sc_trace_file *trace_fp = nullptr;

#ifdef ENABLE_TRACING
    trace_fp = sc_create_vcd_trace_file("pcie_system_trace");
    if (trace_fp)
    {
        // Trace reset signal
        sc_trace(trace_fp, g_top->rst, "rst");

        // Trace interrupt signals
        for (int i = 0; i < g_top->irq_signals.size(); i++)
        {
            std::string sig_name = "irq_" + std::to_string(i);
            sc_trace(trace_fp, g_top->irq_signals[i], sig_name);
        }

        std::cout << "[sc_main] VCD tracing enabled -> pcie_system_trace.vcd" << std::endl;
    }
#endif

    // ============================================
    // Reset Sequence
    // ============================================
    std::cout << "[sc_main] Asserting reset..." << std::endl;
    g_top->rst.write(true);
    sc_start(1, SC_US);

    std::cout << "[sc_main] De-asserting reset..." << std::endl;
    g_top->rst.write(false);
    sc_start(1, SC_US);

    // ============================================
    // Wait for QEMU Connection
    // ============================================
    std::cout << "==================================================" << std::endl;
    std::cout << "Waiting for QEMU to connect..." << std::endl;
    std::cout << "Start QEMU with Remote-Port configuration:" << std::endl;
    std::cout << "  -chardev socket,id=rp0,path=/tmp/qemu-rp-0,server=off" << std::endl;
    std::cout << "  -machine remote-port-adapter=rp0" << std::endl;
    std::cout << "==================================================" << std::endl;

    // ============================================
    // Run Simulation
    // ============================================
    std::cout << "[sc_main] Starting simulation..." << std::endl;

    try
    {
        // Run indefinitely until QEMU disconnects or Ctrl+C
        sc_start();

        std::cout << "[sc_main] Simulation stopped at time: "
                  << sc_time_stamp() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[sc_main] Exception caught: " << e.what() << std::endl;
    }

// ============================================
// Cleanup
// ============================================
#ifdef ENABLE_TRACING
    if (trace_fp)
    {
        sc_close_vcd_trace_file(trace_fp);
        std::cout << "[sc_main] VCD trace file closed" << std::endl;
    }
#endif

    delete g_top;
    g_top = nullptr;

    std::cout << "==================================================" << std::endl;
    std::cout << "Simulation completed successfully" << std::endl;
    std::cout << "==================================================" << std::endl;

    return EXIT_SUCCESS;
}