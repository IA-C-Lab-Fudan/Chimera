#ifndef __FPGA_CHIMERA_CDMA_HH__
#define __FPGA_CHIMERA_CDMA_HH__

#include "common.hh"

namespace gem5
{
namespace fpga
{

class Chimera;
class CDMA
{
  private:
    uint64_t     m_addr;
    uint64_t     m_size;
    uint8_t*     m_buffer;
    uint64_t     m_wptr;
    uint64_t     m_rptr;
    bool         m_status;
    Chimera*     m_parent;
    PCIeRespPkt* m_osdRespPkt;

  public:
    CDMA(Chimera* parent, uint64_t limit_batch_size);
    ~CDMA()
    {
    }

    void enable(uint64_t addr, uint64_t size = 0);
    void disable();
    void execute();

    void fetchData(void* buf, uint64_t size);

    uint64_t getWPtr()
    {
        return m_wptr;
    }


    uint64_t getRPtr()
    {
        return m_rptr;
    }

    int64_t getRemain()
    {
        return m_wptr - m_rptr;
    }

    uint64_t getSize()
    {
        return m_size;
    }

    uint64_t getAddr()
    {
        return m_addr;
    }

    bool getStatus()
    {
        return m_status;
    }
};

} // namespace fpga
} // namespace gem5

#endif