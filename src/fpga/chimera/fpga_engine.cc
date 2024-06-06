#include "base/logging.hh"
#include "fpga/chimera/fpga_engine.hh"

#include <math.h>
#include <iostream>

namespace gem5
{
namespace fpga
{

FPGAEngine::FPGAEngine()
{
    const char* wstr    = "/dev/xdma0_h2c_0";
    m_writeEngineDevice = open(wstr, O_RDWR);
    if (m_writeEngineDevice < 0) {
        std::cout << m_writeEngineDevice << std::endl;
        panic("*** ERROR: failed to open device /dev/xdma0_h2c_0\n");
    } else {
        DPRINTF(FPGAEngine, "SUCCESS: open write engine device\n");
    }

    const char* rstr   = "/dev/xdma0_c2h_0";
    m_readEngineDevice = open(rstr, O_RDWR);
    if (m_readEngineDevice < 0) {
        panic("*** ERROR: failed to open device /dev/xdma0_c2h_0\n");
    } else {
        DPRINTF(FPGAEngine, "SUCCESS: open read engine device\n");
    }
}

FPGAEngine::~FPGAEngine()
{
}

int FPGAEngine::dev_write(uint64_t addr, uint64_t size, void* msg)
{
    if (addr != lseek(m_writeEngineDevice, addr, SEEK_SET)) { panic("*** ERROR: failed to set address of /dev/xdma0_h2c_0\n"); }
    long nanosecond = get_system_time_nanosecond();
    if (size != write(m_writeEngineDevice, (void*)msg, size)) {
        panic("*** ERROR: failed to set size of /dev/xdma0_h2c_0\n");
    } else {
        DPRINTF(FPGAEngine, "SUCCESS: finish once fpga write, address: %#x, size: %d\n", addr, size);
    }
    nanosecond = get_system_time_nanosecond() - nanosecond;
    DPRINTF(FPGAEngine, "pcie write time: %lu\n", nanosecond);
    return 0;
}

int FPGAEngine::dev_read(uint64_t addr, uint64_t size, void* msg)
{
    if (addr != lseek(m_readEngineDevice, addr, SEEK_SET)) { panic("*** ERROR: failed to set address of /dev/xdma0_c2h_0\n"); }
    long nanosecond = get_system_time_nanosecond();
    if (size != read(m_readEngineDevice, (void*)msg, size)) {
        panic("*** ERROR: failed to set size of /dev/xdma0_c2h_0\n");
    } else {
        DPRINTF(FPGAEngine, "SUCCESS: finish once fpga read, address: %#x, size: %d\n", addr, size);
    }
    nanosecond = get_system_time_nanosecond() - nanosecond;
    DPRINTF(FPGAEngine, "pcie read time: %lu\n", nanosecond);
    return 0;
}

void FPGAEngine::dev_init()
{
    void* msg = static_cast<void*>(std::malloc(32));
    dev_write(0x1000, 32, msg);
    free(msg);
}

void FPGAEngine::config_read_mode_poll()
{
    void* msg = static_cast<void*>(std::malloc(32));
    dev_write(0x1008, 32, msg);
    free(msg);
}

void FPGAEngine::dev_stop()
{
    void* msg = static_cast<void*>(std::malloc(32));
    dev_write(0x2000, 32, msg);
    free(msg);
}

void FPGAEngine::printPCIeTime()
{
    for(int i = 0; i < m_pcie_read_time.size(); ++i) {
        std::cout << "pcie_read_time: " << m_pcie_read_time.front() << std::endl;
        m_pcie_read_time.pop_front();
    }

    for (int j = 0; j < m_pcie_write_time.size(); ++j) {
        std::cout << "pcie_write_time: " << m_pcie_write_time.front() << std::endl;
        m_pcie_write_time.pop_front();
    }
}

} // namespace fpga
} // namespace gem5