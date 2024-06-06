#ifndef __FPGA_CHIMERA_FPGA_ENGINE_HH__
#define __FPGA_CHIMERA_FPGA_ENGINE_HH__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/timeb.h>
#include <string>

#include "base/trace.hh"
#include "debug/FPGAEngine.hh"

#include "fpga/chimera/utils.hh"

namespace gem5
{
namespace fpga
{

class FPGAEngine
{
  private:
    int m_writeEngineDevice;
    int m_readEngineDevice;

    std::list<uint64_t> m_pcie_write_time;
    std::list<uint64_t> m_pcie_read_time;

  public:
    FPGAEngine();
    ~FPGAEngine();

    void dev_init();
    void dev_stop();
    int  dev_write(uint64_t addr, uint64_t size, void* msg);
    int  dev_read(uint64_t addr, uint64_t size, void* msg);

    void config_read_mode_poll();
    void printPCIeTime();
};

} // namespace fpga
} // namespace gem5

#endif