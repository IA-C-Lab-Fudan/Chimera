#include <iostream>
#include <random>
#include <chrono>
#include <assert.h>
#include <unistd.h>
#include <condition_variable>
#include <atomic>

#include "fpga/chimera/chimera.hh"
#include "fpga/mpeg2/mpeg2_encoder.hh"

namespace gem5
{
namespace fpga
{

std::mutex auxMTX;
std::mutex readMTX;
std::mutex fetchMTX;
std::mutex writeMTX;

std::condition_variable auxCV;
std::condition_variable writeCV;
std::condition_variable readCV;
std::condition_variable fetchCV;


Chimera::Chimera(const ChimeraParams& p) :
    ClockedObject(p),
    m_taskTableNum(p.taskTableNum),
    m_idleTaskTableID(p.taskTableNum),
    m_totalTaskID(0),
    m_cdma_enable(p.enableCDMA)
{
    completeListUpdated = false;
    auxDone.store(false, std::memory_order_relaxed);
    writeDone.store(false, std::memory_order_relaxed);
    readDone.store(false, std::memory_order_relaxed);
    osdTaskCounter.store(0, std::memory_order_relaxed);
    comeInList     = new RingBuffer<PCIeTask>(p.taskTableNum);
    ready2Transmit = new RingBuffer<PCIeTask>(p.taskTableNum);
    ready2Response = new RingBuffer<PCIeRespPkt>(p.taskTableNum);

    m_validTaskTableNum.store(p.taskTableNum, std::memory_order_relaxed);
    m_osdReqPkt  = new PCIeReqPkt(1);
    m_osdDataPkt = new PCIeDataPkt(1);
    m_osdRespPkt = new PCIeRespPkt(1);

    for (int i = 0; i < p.taskTableNum; ++i) {
        m_taskTable.push_back(new taskTableEntry(i));
        assert(m_idleTaskTableID.enqueue(i));
    }

    m_fpga         = new FPGAEngine();
    m_cdma         = new CDMA(this, 1);
    m_parent_retry = false;
    m_fpga->dev_init();
    m_fpga->config_read_mode_poll();

    // set batch threshold
    int* msg = static_cast<int*>(std::malloc(32));
    *msg = 1;
    m_fpga->dev_write(0x1010, 32, msg);
    free(msg);

    auxThread   = std::thread(&Chimera::auxThreadFunc, this);
    readThread  = std::thread(&Chimera::collectThreadFunc, this);
    writeThread = std::thread(&Chimera::submitThreadFunc, this);

    std::string s1 = "chimera";
    m_stats = new ChimeraStats(*this, s1);
    m_lifeCycleTable.resize(m_taskTableNum);

    if (p.enable_log) {
        registerExitCallback([this]() { printBatchRecord(); });
        registerExitCallback([this]() { m_fpga->printPCIeTime(); });
    }
}

Chimera::~Chimera()
{
}

void Chimera::submitThreadFunc()
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    int threadCore = 7;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);                               
    CPU_SET(threadCore, &cpuset);                    
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    while (true) {
        DPRINTF(Chimera, "[writeThread] acquiring for locks\n");
        std::unique_lock<std::mutex> lock(writeMTX);
        writeCV.wait(lock, [this] { return !ready2Transmit->isEmpty() || writeDone.load(std::memory_order_relaxed); });
        DPRINTF(Chimera, "[writeThread] active, ready to work\n");
        if (ready2Transmit->isEmpty() && writeDone.load(std::memory_order_relaxed)) { break; }
        if (!ready2Transmit->isEmpty()) {
            std::vector<int8_t> submitTaskTableID;

            uint8_t lastTaskAttr = ready2Transmit->peek().m_basic;
            
            if (lastTaskAttr == ready2Transmit->peek().m_basic) {
                if (ready2Transmit->peek().isWrite()) {
                    PCIeTask task = ready2Transmit->peek();
                    lastTaskAttr  = task.m_basic;
                    m_osdReqPkt->fillTask(0, task);
                    ready2Transmit->dequeue();
                    submitTaskTableID.push_back(task.m_tableID);
                } else if (ready2Transmit->peek().isData()) {
                    PCIeTask task = ready2Transmit->peek();
                    lastTaskAttr  = task.m_basic;
                    if (m_osdDataPkt->fillTask(0, task) == 0) {
                        m_osdReqPkt->fillTask(0, task);
                        ready2Transmit->dequeue();
                        submitTaskTableID.push_back(task.m_tableID);
                    } else {
                        break;
                    }
                } else if (ready2Transmit->peek().isRead()) {
                    assert(false);
                    break;
                } else {
                    assert(false);
                }
            } else {
                break;
            }

            m_osdReqPkt->m_valid = 0x1;
            m_osdReqPkt->m_batch = 1;

            lock.unlock();

            DPRINTF(Chimera, "[writeThread] ready to issue pcie write request\n");

            if (lastTaskAttr & 0x4) {
                for (int i = 0; i < submitTaskTableID.size(); ++i) {
                    m_lifeCycleTable[submitTaskTableID[i]].m_pre_submitTime = get_system_time_nanosecond();
                }

                m_fpga->dev_write(0x0, m_osdReqPkt->m_size, m_osdReqPkt);

                for (int i = 0; i < submitTaskTableID.size(); ++i) {
                    m_lifeCycleTable[submitTaskTableID[i]].m_post_submitTime = get_system_time_nanosecond();
                }

                if (lastTaskAttr & 0x2) {
                    [[maybe_unused]] int value = osdTaskCounter.fetch_add(m_osdReqPkt->m_batch, std::memory_order_relaxed);
                    readCV.notify_one();
                    DPRINTF(Chimera, "the request need response, notify read thread\n");
                } else {
                    for (int i = 0; i < m_osdReqPkt->m_batch; ++i) {
                        int tableID = m_osdReqPkt->m_tasks[i].m_tableID;
                    
                        submitLifeCycle(tableID);

                        assert(m_idleTaskTableID.enqueue(tableID));

                        parentRetryCallback();

                        m_taskTable[tableID]->m_valid = 0x0;
                        m_validTaskTableNum.fetch_add(1, std::memory_order_relaxed);
                        DPRINTF(Chimera, "the request dont need response, release task table entry: %d\n", tableID);
                    }
                }
            } else if (lastTaskAttr & 0x8) {
                [[maybe_unused]] int value = osdTaskCounter.fetch_add(m_osdReqPkt->m_batch, std::memory_order_relaxed);
                readCV.notify_one();
                DPRINTF(Chimera, "the request need response, notify read thread\n");
            } else if (lastTaskAttr & 0x10) {
                m_fpga->dev_write(m_osdDataPkt->getAddr(), m_osdDataPkt->getSize(), m_osdDataPkt->getDataPtr());

                for (int i = 0; i < m_osdReqPkt->m_batch; ++i) {
                    int tableID = m_osdReqPkt->m_tasks[i].m_tableID;
                    
                    submitLifeCycle(tableID);

                    assert(m_idleTaskTableID.enqueue(tableID));

                    parentRetryCallback();

                    m_taskTable[tableID]->m_valid = 0x0;
                    m_validTaskTableNum.fetch_add(1, std::memory_order_relaxed);

                    issued_data_packet++;
                    DPRINTF(Chimera, "the request dont need response, release task table entry: %d\n", tableID);
                }
            } else {
                assert(false);
            }

            DPRINTF(Chimera, "[writeThread] pcie write request finished\n");
            m_osdReqPkt->m_valid = 0x0;
        }
    }
    DPRINTF(Chimera, "WRITE Thread Exit...\n");
}

void Chimera::collectThreadFunc()
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    int threadCore = 6;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);                               
    CPU_SET(threadCore, &cpuset);                    
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    while (true) {
        DPRINTF(
            Chimera, "[readThread] acquiring for locks, osdTaskCounter: %d, readyDone: %d\n",
            osdTaskCounter.load(std::memory_order_relaxed), readDone.load(std::memory_order_relaxed));
        std::unique_lock<std::mutex> lock(readMTX);
        readCV.wait(
            lock, [this] { return osdTaskCounter.load(std::memory_order_relaxed) > 0 || readDone.load(std::memory_order_relaxed); });


        if (readDone.load(std::memory_order_relaxed)) {
            DPRINTF(Chimera, "[readThread] readDone, this thread ready to exit\n");
            break;
        }

        if (!m_cdma_enable) {
            DPRINTF(Chimera, "[readThread] issuing pcie read request\n");
            uint64_t pre_latency = get_system_time_nanosecond();

            m_fpga->dev_read(0x0, m_osdRespPkt->m_size, m_osdRespPkt);

            [[maybe_unused]] int value = osdTaskCounter.fetch_sub(m_osdRespPkt->m_batch, std::memory_order_relaxed);


            DPRINTF(Chimera, "------RESPONSE DETAIL--------\n");
            DPRINTF(Chimera, "batch: %d\n", m_osdRespPkt->m_batch);
            DPRINTF(Chimera, "task1.tableID: %d\n", m_osdRespPkt->m_results[0].m_tableID);
            DPRINTF(Chimera, "task2.tableID: %d\n", m_osdRespPkt->m_results[1].m_tableID);
            DPRINTF(Chimera, "---------------------------\n");

            if (m_osdRespPkt->m_valid == 0x1) {
                for (int i = 0; i < m_osdRespPkt->m_batch; ++i) {
                    m_lifeCycleTable[m_osdRespPkt->m_results[i].m_tableID].m_pre_collectTime  = pre_latency;
                    m_lifeCycleTable[m_osdRespPkt->m_results[i].m_tableID].m_post_collectTime = get_system_time_nanosecond();
                }
                DPRINTF(Chimera, "[readThread] fetch valid response, notify auxThread\n");
                assert(ready2Response->enqueue(*m_osdRespPkt));

                DPRINTF(Chimera, "[readThread] to notify auxThread\n");
                auxCV.notify_one();
            } else {
                DPRINTF(Chimera, "[readThread] fetch invalid response, ready to fetch again\n");
            }

            m_osdRespPkt->m_valid = 0x0;
        } else {
            m_cdma->execute();
            disableCDMA();
        }
    }

    assert(osdTaskCounter.load(std::memory_order_relaxed) == 0);
    DPRINTF(Chimera, "READ Thread Exit...\n");
}

void Chimera::auxThreadFunc()
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    int threadCore = 5;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);                               
    CPU_SET(threadCore, &cpuset);                    
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    while (true) {
        DPRINTF(Chimera, "[auxThread] acquiring for locks\n");
        std::unique_lock<std::mutex> lock(auxMTX);
        auxCV.wait(lock, [this] { return !comeInList->isEmpty() || !ready2Response->isEmpty() || auxDone; });
        DPRINTF(Chimera, "[auxThread] active, ready to work\n");
        if (!comeInList->isEmpty()) {
            DPRINTF(Chimera, "[auxThread] comeInList has outstanging task, ready to fetch\n");

            PCIeTask handlingTask = comeInList->dequeue();
            int      tableID      = handlingTask.m_tableID;

            m_taskTable[tableID]->m_valid   = 0x1;
            m_taskTable[tableID]->m_taskUID = m_totalTaskID++;
            m_taskTable[tableID]->m_task = handlingTask;

            m_validTaskTableNum.fetch_sub(1, std::memory_order_relaxed);
            assert(ready2Transmit->enqueue(handlingTask));
            DPRINTF(Chimera, "[auxThread] alloc task table entry\n");
            DPRINTF(Chimera, "[auxThread] insert the task into ready2transimit and notify write thread\n");

            DPRINTF(Chimera, "[auxThread] to notify writeThread\n");
            writeCV.notify_one();
        }

        if (!ready2Response->isEmpty()) {
            PCIeRespPkt pkt = ready2Response->dequeue();

            std::list<int> ready2CompleteList;
            for (int i = 0; i < pkt.m_batch; ++i) {
                int tableID = pkt.m_results[i].m_tableID;
                
                m_lifeCycleTable[tableID].m_respTime = get_system_time_nanosecond();
                m_lifeCycleTable[tableID].m_post_hwTime = pkt.m_results[i].m_executedTime;
                submitLifeCycle(tableID);

                assert(m_idleTaskTableID.enqueue(tableID));

                parentRetryCallback();

                m_taskTable[tableID]->m_result   = pkt.m_results[i];
                m_taskTable[tableID]->m_complete = true;
                ready2CompleteList.push_back(tableID);
            }

            std::lock_guard<std::mutex> lock(fetchMTX);
            completeList.splice(completeList.end(), ready2CompleteList);
            completeListUpdated = true;
            DPRINTF(Chimera, "[auxThread] insert the task into completeList and notify gem5 thread\n");

            DPRINTF(Chimera, "[auxThread] to notify gem5Thread\n");
            fetchCV.notify_one();
        }

        if (auxDone.load(std::memory_order_relaxed)) {
            if (!writeDone.load(std::memory_order_relaxed) && comeInList->isEmpty()) {
                writeDone.store(true, std::memory_order_relaxed);

                DPRINTF(Chimera, "[auxThread] to notify writeThread\n");
                writeCV.notify_one();
            }

            if (!readDone.load(std::memory_order_relaxed) && m_validTaskTableNum.load(std::memory_order_relaxed) == m_taskTableNum) {
                readDone.store(true, std::memory_order_relaxed);

                DPRINTF(Chimera, "[auxThread] to notify readThread\n");
                readCV.notify_one();
            } else {
                DPRINTF(
                    Chimera, "readyDone: %d, m_validTaskTableNum: %d\n", readDone.load(std::memory_order_relaxed),
                    m_validTaskTableNum.load(std::memory_order_relaxed));
                while (!m_idleTaskTableID.isEmpty()) { DPRINTF(Chimera, "remain valid table id: %d\n", m_idleTaskTableID.dequeue()); }
            }
            sleep(2);
        }

        if (readDone.load(std::memory_order_relaxed) && writeDone.load(std::memory_order_relaxed)
            && auxDone.load(std::memory_order_relaxed)) {
            break;
        }
    }
    DPRINTF(Chimera, "AUX Thread Exit...\n");
}

bool Chimera::isFullAndMark()
{
    bool result = m_idleTaskTableID.isEmpty();
    if (result) { m_parent_retry = true; }
    return result;
}

int Chimera::recvTask(PCIeTask task)
{
    while (true) {
        if (!m_idleTaskTableID.isEmpty()) { break; }
    }

    int id = m_idleTaskTableID.dequeue();

    assert(task.isValid());
    task.m_tableID = id;

    m_taskTable[id]->m_enqueueTime = curCycle();

    assert(!m_lifeCycleTable[id].m_valid);

    m_lifeCycleTable[id].m_valid    = true;
    m_lifeCycleTable[id].m_recvTime = get_system_time_nanosecond();
    m_lifeCycleTable[id].m_pre_hwTime = task.m_insertTime;

    assert(comeInList->enqueue(task));
    DPRINTF(Chimera, "insert the task into comeInList (id: %d), notify auxThread\n", id);

    DPRINTF(Chimera, "[gem5Thread] to notify auxThread\n");
    auxCV.notify_one();
    return id;
}

void Chimera::simBegin()
{
    m_fpga->dev_init();
}

void Chimera::simExit()
{
    auxDone.store(true, std::memory_order_relaxed);

    DPRINTF(Chimera, "[gem5Thread] to notify auxThread (ready to exit)\n");
    auxCV.notify_one();

    auxThread.join();
    readThread.join();
    writeThread.join();
}

std::pair<PCIeResult, uint64_t> Chimera::fetchResp(int id)
{
    while (true) {
        DPRINTF(Chimera, "[gem5Thread] trying to fetch response\n");

        bool       findResponse = false;

        std::pair<PCIeResult, uint64_t> final;

        std::unique_lock<std::mutex> lock(fetchMTX);
        for (std::list<int>::iterator it = completeList.begin(); it != completeList.end(); ++it) {
            completeListUpdated = false;
            if (*it == id) {
                findResponse = true;
                final.first              = m_taskTable[id]->m_result;
                final.second             = m_taskTable[id]->m_enqueueTime;

                m_taskTable[id]->m_valid = 0x0;
                m_validTaskTableNum.fetch_add(1, std::memory_order_relaxed);

                DPRINTF(Chimera, "[auxThread] release task table entry\n");

                completeList.erase(it);

                DPRINTF(Chimera, "[gem5Thread] to notify auxThread\n");
                auxCV.notify_one();
                break;
            }
        }

        if (!findResponse) {
            DPRINTF(Chimera, "[gem5Thread] dont find response, ready to sleep\n");
            fetchCV.wait(lock, [this] { return !completeList.empty() && completeListUpdated; });
        } else {
            DPRINTF(Chimera, "[gem5Thread] find the response, return it\n");
            return final;
        }
    }
}

void Chimera::parentRetryCallback()
{
    if (m_parent_retry) {
        if (m_parent_name == "mpeg2encoder") {
            Mpeg2Encoder* parent = static_cast<Mpeg2Encoder*>(m_parent);
            parent->retryCallback();
        } else {
            assert(false);
        }
        m_parent_retry = false;
    }
}

void Chimera::enableCDMA(uint64_t addr, uint64_t size)
{
    m_cdma->enable(addr, size);
    [[maybe_unused]] int value = osdTaskCounter.fetch_add(1, std::memory_order_relaxed);
    readCV.notify_one();
}

void Chimera::disableCDMA()
{
    m_cdma->disable();
    [[maybe_unused]] int value = osdTaskCounter.fetch_sub(m_osdRespPkt->m_batch, std::memory_order_relaxed);
    readCV.notify_one();
}


Chimera::ChimeraStats::ChimeraStats(Chimera& c, const std::string& name) :
    statistics::Group(&c), parent(c),
    ADD_STAT(handleCount, statistics::units::Count::get(), "handled task count"),
    ADD_STAT(preFlow, statistics::units::Count::get(), "recv task -> pcie write"),
    ADD_STAT(postFlow, statistics::units::Count::get(), "pcie read -> send resp"),
    ADD_STAT(pcieSubmit, statistics::units::Count::get(), "pcie write"),
    ADD_STAT(pcieCollect, statistics::units::Count::get(), "pcie read (warn)"),
    ADD_STAT(hardwareExecution, statistics::units::Count::get(), "hardware execution time")
{

}

void
Chimera::ChimeraStats::regStats()
{
    using namespace statistics;

    statistics::Group::regStats();

    handleCount.flags(total | nozero);
    preFlow.flags(total | nozero);
    postFlow.flags(total | nozero);
    pcieSubmit.flags(total | nozero);
    pcieCollect.flags(total | nozero);
    hardwareExecution.flags(total | nozero);
}

void
Chimera::printBatchRecord()
{
    for (int i = 0; i < m_rdBatch_record.size(); ++i) {
        std::cout << "rdBatch_record: " << m_rdBatch_record.front() << std::endl;
        m_rdBatch_record.pop_front();
    }

    for (int j = 0; j < m_wrBatch_record.size(); ++j) {
        std::cout << "wrBatch_record: " << m_wrBatch_record.front() << std::endl;
        m_wrBatch_record.pop_front();
    }
}

} // namespace fpga
} // namespace gem5