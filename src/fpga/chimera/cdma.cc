#include "fpga/chimera/cdma.hh"
#include "fpga/chimera/chimera.hh"

namespace gem5
{
namespace fpga
{

CDMA::CDMA(Chimera* parent, uint64_t limit_batch_size)
{
    m_addr       = 0;
    m_size       = 0;
    m_buffer     = new uint8_t[0x200000];
    m_wptr       = 0;
    m_rptr       = 0;
    m_status     = false;
    m_parent     = parent;
    m_osdRespPkt = new PCIeRespPkt(limit_batch_size);
}

void CDMA::enable(uint64_t addr, uint64_t size)
{
    assert(!m_status);
    m_addr   = addr;
    m_size   = size;
    m_wptr   = 0;
    m_rptr   = 0;
    m_status = true;
}

void CDMA::disable()
{
    assert(m_status);
    m_addr   = 0;
    m_size   = 0;
    m_wptr   = 0;
    m_rptr   = 0;
    m_status = false;
}

void CDMA::execute()
{
    uint64_t count = 0;
    if (m_status) {
        while (true) {
            m_parent->getFPGAEngine()->dev_read(m_addr + m_wptr, m_osdRespPkt->m_size, m_osdRespPkt);
            if (m_osdRespPkt->m_valid & 0x1) {
                uint64_t batch = m_osdRespPkt->m_batch;
                count++;
                for (int i = 0; i < batch; ++i) {
                    if (m_osdRespPkt->m_results[i].m_valid & 0x2) {
                        std::cout << "count: " << count << std::endl;
                    }
                    std::memcpy(m_buffer + m_wptr, &(m_osdRespPkt->m_results[i].m_content), RESULT_DATA_SIZE);
                    m_wptr += RESULT_DATA_SIZE;
                }
            } else {
                // hardware is not ready for response
            }
        }
    }
}

void CDMA::fetchData(void* buf, uint64_t size)
{
    if (size <= getRemain()) {
        std::memcpy(buf, m_buffer + m_rptr, size);
        m_rptr += size;
    } else {
        std::memset(buf, 0, size);
    }
}

} // namespace fpga
} // namespace gem5