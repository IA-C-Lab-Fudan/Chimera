#ifndef __FPGA_CHIMERA_COMMON_HH__
#define __FPGA_CHIMERA_COMMON_HH__

#include <iostream>
#include <vector>
#include <assert.h>
#include <cstring>

namespace gem5
{
namespace fpga
{

#define MAX_BATCH_THRESHOLD 3
#define TASK_HEADER_SIZE 10
#define TASK_CTRL_DATA_SIZE 16
#define TASK_CTRL_PKT_SIZE ((TASK_HEADER_SIZE) + (TASK_CTRL_DATA_SIZE))
#define TASK_DATA_SIZE 64
#define RESULT_DATA_SIZE 32

#pragma pack(1)

struct PCIeCtrlTask {
    uint8_t  m_basic;                        // [7:0]  0: valid; 1: needResp; 2: write; 3: read; 4: data, 5~7: reserved
    int8_t   m_tableID;                      // [15:8]
    uint64_t m_insertTime;                   // [79:16]
    uint8_t  m_content[TASK_CTRL_DATA_SIZE]; // [143:80] [207:144]
};

struct PCIeDataTask {
    uint32_t m_addr;
    uint32_t m_size;
    uint8_t  m_content[TASK_DATA_SIZE];
};

struct PCIeTask {
    uint8_t  m_basic;      // [7:0]  0: valid; 1: needResp; 2: write; 3: read; 4: data, 5~7: reserved
    int8_t   m_tableID;    // [15:8]
    uint64_t m_insertTime; // [79:16]
    uint8_t  m_content[TASK_DATA_SIZE];
    uint32_t m_addr;
    uint32_t m_size;

    PCIeTask()
    {
        m_basic      = 0x0;
        m_tableID    = -1;
        m_insertTime = 0;
        m_addr       = 0;
        m_size       = 0;
        std::memset(&m_content, 0, TASK_DATA_SIZE);
    }

    bool isValid()
    {
        return m_basic & 0x1;
    }

    bool isNeedResp()
    {
        return m_basic & 0x2;
    }

    bool isWrite()
    {
        return m_basic & 0x4;
    }

    bool isRead()
    {
        return m_basic & 0x8;
    }

    bool isCtrl()
    {
        return isRead() || isWrite();
    }

    bool isData()
    {
        return m_basic & 0x10;
    }

    void setValid()
    {
        m_basic |= 1 << 0;
    }

    void unsetValid()
    {
        m_basic &= 0 << 0;
    }

    void setNeedResp()
    {
        m_basic |= 1 << 1;
    }

    void setWriteType()
    {
        m_basic &= ~(1ULL << 1);
        m_basic |= 1 << 2;
    }

    void setReadType()
    {
        m_basic &= ~(1ULL << 1);
        m_basic |= 1 << 3;
    }

    void setDataType()
    {
        m_basic &= ~(1ULL << 1);
        m_basic |= 1 << 4;
    }

    void fillData(uint32_t start_addr, void* data, uint32_t size)
    {
        if (isCtrl()) {
            assert(start_addr + size <= TASK_CTRL_DATA_SIZE);
            std::memcpy(&m_content[0], data, size);
        } else if (isData()) {
            m_addr = start_addr;
            m_size = size;
            std::memcpy(&m_content[0], data, size);
        } else {
            assert(false);
        }
    }
};

// 208bit
struct PCIeResult {
    uint8_t  m_valid;        // [7:0]
    int8_t   m_tableID;      // [15:8]
    uint64_t m_executedTime; // [79:16]
    uint8_t  m_content[RESULT_DATA_SIZE];
};

struct LifeCycle
{
    bool     m_valid;
    bool     m_needResp;
    uint64_t m_recvTime;
    uint64_t m_pre_submitTime;
    uint64_t m_post_submitTime;
    uint64_t m_pre_collectTime;
    uint64_t m_post_collectTime;
    uint64_t m_respTime;
    uint64_t m_pre_hwTime;
    uint64_t m_post_hwTime;
    
    LifeCycle()
    {
        reset();
    }

    void reset()
    {
        m_valid            = false;
        m_needResp         = false;
        m_recvTime         = 0;
        m_pre_submitTime   = 0;
        m_post_submitTime  = 0;
        m_pre_collectTime  = 0;
        m_post_collectTime = 0;
        m_respTime         = 0;
        m_pre_hwTime       = 0;
        m_post_hwTime      = 0;
    }
};

struct taskTableEntry {
    bool       m_valid;
    bool       m_complete;
    int8_t     m_tableID;
    uint64_t   m_taskUID;
    PCIeTask   m_task;
    PCIeResult m_result;
    uint64_t   m_enqueueTime;

    taskTableEntry(int tableID)
    {
        m_valid       = false;
        m_complete    = false;
        m_tableID     = tableID;
        m_enqueueTime = 0;
    }
};

struct alignas(16) PCIeReqPkt {
    uint8_t      m_valid;
    uint8_t      m_batch;
    PCIeCtrlTask m_tasks[MAX_BATCH_THRESHOLD];
    uint64_t     m_size;

    PCIeReqPkt(int limit_batch_size)
    {
        m_valid = 0x0;
        m_size  = 1 + 1 + sizeof(PCIeCtrlTask) * limit_batch_size;
    }

    void fillTask(int index, PCIeTask task)
    {
        m_tasks[index].m_basic      = task.m_basic;
        m_tasks[index].m_tableID    = task.m_tableID;
        m_tasks[index].m_insertTime = task.m_insertTime;
        std::memcpy(&(m_tasks[index].m_content[0]), &(task.m_content[0]), TASK_CTRL_DATA_SIZE);
    }
};

struct alignas(16) PCIeDataPkt {
    uint32_t m_start_addr;
    uint32_t m_total_size;
    uint8_t  m_data[MAX_BATCH_THRESHOLD * TASK_DATA_SIZE];
    uint64_t m_size;

    PCIeDataPkt(int limit_batch_size)
    {
        m_start_addr = 0;
        m_total_size = 0;
        std::memset(&m_data, 0, MAX_BATCH_THRESHOLD * TASK_DATA_SIZE);
        m_size = 4 + 4 + TASK_DATA_SIZE * limit_batch_size;
    }

    int fillTask(int index, PCIeTask task)
    {
        int result = 0;
        if (index == 0) {
            m_start_addr = task.m_addr;
            m_total_size = task.m_size;
            std::memcpy(&m_data, &task.m_content, TASK_DATA_SIZE);
            result = 0;
        } else {
            if (m_start_addr + m_total_size != task.m_addr) {
                result = -1;
            } else {
                m_total_size += task.m_size;
                std::memcpy(&m_data + index * TASK_DATA_SIZE, &task.m_content, TASK_DATA_SIZE);
                result = 0;
            }
        }
        return result;
    }

    uint8_t* getDataPtr()
    {
        return m_data;
    }

    uint64_t getAddr()
    {
        return m_start_addr;
    }

    uint64_t getSize()
    {
        return m_total_size;
    }
};

struct alignas(16) PCIeRespPkt {
    uint8_t    m_valid;
    uint8_t    m_batch;
    PCIeResult m_results[MAX_BATCH_THRESHOLD];
    uint64_t   m_size;

    PCIeRespPkt(int limit_batch_size)
    {
        m_valid = 0x0;
        m_size  = 1 + 1 + sizeof(PCIeResult) * limit_batch_size;
    }

    PCIeRespPkt()
    {
        m_valid = 0x0;
        m_size  = 0;
    }
};
#pragma pack()


} // namespace fpga
} // namespace gem5

#endif