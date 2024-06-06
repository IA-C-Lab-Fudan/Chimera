#ifndef __FPGA_CHIMERA_CHIMERA_HH__
#define __FPGA_CHIMERA_CHIMERA_HH__

#include "params/Chimera.hh"
#include "sim/clocked_object.hh"
#include "debug/Chimera.hh"

#include <thread>
#include <mutex>
#include <list>
#include <atomic>

#include "fpga/chimera/common.hh"
#include "fpga/chimera/ringBuffer.hh"
#include "fpga/chimera/fpga_engine.hh"
#include "fpga/chimera/cdma.hh"

#include "base/statistics.hh"

namespace gem5
{
namespace fpga
{

class Chimera : public ClockedObject
{
  private:
    std::thread auxThread;
    std::thread readThread;
    std::thread writeThread;

    std::vector<taskTableEntry*> m_taskTable;
    std::atomic<int>             m_validTaskTableNum;
    int                          m_taskTableNum;

    RingBuffer<int> m_idleTaskTableID;
    uint64_t        m_totalTaskID;

    FPGAEngine* m_fpga;
    CDMA*       m_cdma;
    std::string m_parent_name;
    SimObject*  m_parent;
    bool        m_parent_retry;

    PCIeReqPkt*  m_osdReqPkt;
    PCIeDataPkt* m_osdDataPkt;
    PCIeRespPkt* m_osdRespPkt;

    bool  m_cdma_enable;
    bool  m_pollReadMode_enable;

    std::atomic<bool>        auxDone;
    std::atomic<bool>        writeDone;
    std::atomic<bool>        readDone;
    std::atomic<int>         osdTaskCounter;
    RingBuffer<PCIeTask>*    comeInList;
    RingBuffer<PCIeTask>*    ready2Transmit;
    RingBuffer<PCIeRespPkt>* ready2Response;
    std::list<int>           completeList;
    bool                     completeListUpdated;

    uint64_t issued_data_packet = 0;

  public:
    Chimera(const ChimeraParams& p);
    ~Chimera();

    void setParent(std::string name, SimObject* parent)
    {
        m_parent      = parent;
        m_parent_name = name;
    }

    void auxThreadFunc();
    void collectThreadFunc();
    void submitThreadFunc();

    bool isFullAndMark();
    int  recvTask(PCIeTask task);
    void simBegin();
    void simExit();

    std::pair<PCIeResult, uint64_t> fetchResp(int id);

    void parentRetryCallback();

    FPGAEngine* getFPGAEngine()
    {
        return m_fpga;
    }

    void enableCDMA(uint64_t addr, uint64_t size = 0);
    void disableCDMA();
    bool CDMAStatus()
    {
        return m_cdma->getStatus();
    }

    void CDMAFetchData(void* buf, uint64_t size)
    {
        m_cdma->fetchData(buf, size);
    }

    int64_t CDMARemain()
    {
        int64_t result = m_cdma->getRemain();
        assert(result >= 0);
        return result;
    }

  public:
    struct ChimeraStats : public statistics::Group {
        ChimeraStats(Chimera& c, const std::string& name);
        Chimera& parent;
        void regStats() override;

        statistics::Scalar handleCount;
        statistics::Scalar preFlow;
        statistics::Scalar postFlow;
        statistics::Scalar pcieSubmit;
        statistics::Scalar pcieCollect;
        statistics::Scalar hardwareExecution;
    };

    ChimeraStats*          m_stats;
    std::vector<LifeCycle> m_lifeCycleTable;

    std::list<uint64_t>    m_rdBatch_record;
    std::list<uint64_t>    m_wrBatch_record;

    void submitLifeCycle(int tableID)
    {
        LifeCycle entry             = m_lifeCycleTable[tableID];
        assert(entry.m_valid);
        
        m_stats->handleCount       += 1;
        m_stats->preFlow           += (entry.m_pre_submitTime - entry.m_recvTime);
        m_stats->pcieSubmit        += (entry.m_post_submitTime - entry.m_pre_submitTime);
        m_stats->pcieCollect       += (entry.m_post_collectTime - entry.m_pre_collectTime);
        m_stats->postFlow          += (entry.m_respTime - entry.m_post_collectTime);

        m_lifeCycleTable[tableID].m_valid = false;
    }

    void printBatchRecord();
};

} // namespace fpga
} // namespace gem5


#endif