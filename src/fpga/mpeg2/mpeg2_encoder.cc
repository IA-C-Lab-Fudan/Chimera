#include "fpga/mpeg2/mpeg2_encoder.hh"

namespace gem5
{
namespace fpga
{

Mpeg2Encoder::Mpeg2Encoder(const Mpeg2EncoderParams& p) : ClockedObject(p), m_chimera(p.chimera), m_enable_verilator(p.enable_verilator)
{
    m_cpu_side_port      = new Mpeg2EncoderCpuSidePort("Mpeg2Encoder.cpu_side_port", this);
    m_wakeupEvent        = new EventFunctionWrapper([this] { wakeup(); }, "Wakeup Event");
    m_retryCallbackEvent = new EventFunctionWrapper([this] { submit(); }, "RetryCallback Event");

    m_local_data = 1;

    m_local_node_ptr  = 0;
    m_local_node_addr = 0;
    m_local_node      = new uint8_t[TASK_DATA_SIZE];

    m_stalling_packet = nullptr;

    if (m_enable_verilator) {
        wr               = new Wrapper_mpeg2(p.dump_wave, "mpeg2_trace.vcd");
        m_buffer         = new uint8_t[0x200000];
        m_wptr           = 0;
        m_rptr           = 0;
        m_last_signal    = false;
        m_stop_verilator = false;
        m_VwakeupEvent   = new EventFunctionWrapper([this] { Vwakeup(); }, "Vwakeup Event");
        schedule(m_VwakeupEvent, nextCycle());
    } else {
        wr = nullptr;
    }

    m_enable_dataPlane_opt = p.enable_dataPlane_opt;
    if (m_enable_dataPlane_opt) {
        assert(!m_enable_verilator);
    }
}

Mpeg2Encoder::~Mpeg2Encoder()
{
}

Port& Mpeg2Encoder::getPort(const std::string& if_name, PortID idx)
{
    if (if_name == "cpu_side_port")
        return *m_cpu_side_port;
    else
        assert(false);
}
void Mpeg2Encoder::init()
{
    if (!m_cpu_side_port->isConnected()) fatal("Both ports of a bridge must be connected.\n");

    m_cpu_side_port->sendRangeChange();

    if (m_chimera) { m_chimera->setParent("mpeg2encoder", this); }
}

void Mpeg2Encoder::retryCallback()
{
    if (m_data_nodes.size() > 0 && m_stop_signal.isValid()) {
        submit();
    }
}

void Mpeg2Encoder::submit()
{
    while (m_data_nodes.size() > 0) {
        PCIeTask task;
        task.setValid();
        task.setDataType();
        if (!m_chimera->isFullAndMark()) {
            uint64_t send_addr = m_data_nodes.front().first;
            uint8_t* sent_data = m_data_nodes.front().second;
            task.m_addr        = send_addr;
            task.m_size        = TASK_DATA_SIZE;
            std::memcpy(&(task.m_content[0]), sent_data, TASK_DATA_SIZE);
            assert(m_chimera->recvTask(task) != -1);
            delete[] sent_data;
            m_data_nodes.pop_front();
        } else {
            break;
        }
    }

    if (m_data_nodes.size() == 0 && m_stop_signal.isValid()) {
        assert(m_chimera->recvTask(m_stop_signal) != -1);
        m_stop_signal.unsetValid();
    }
}

void Mpeg2Encoder::wakeup()
{
    if (m_pending_packets.size() > 0 && m_stalling_packet == nullptr) {
        PacketPtr pkt = m_pending_packets.front();
        if (pkt->isRead()) {
            uint64_t config_addr = pkt->getAddr() - MPEG2_BASE_ADDR;
            if (config_addr == 0x000000010) {
                uint64_t size = pkt->getSize();
                assert(size == 8);
                uint8_t* new_data = new uint8_t[pkt->getSize()];
                uint64_t value    = 0;
                value |= 0x1;
                value |= ((!m_chimera->CDMAStatus()) << 2);
                value |= (m_chimera->CDMARemain() << 32);
                std::memcpy(new_data, &value, sizeof(uint64_t));
                pkt->setData(new_data);
                delete[] new_data;
            } else if (config_addr >= 0x010000000) {
                uint8_t* new_data = new uint8_t[pkt->getSize()];
                m_chimera->CDMAFetchData(new_data, pkt->getSize());
                pkt->setData(new_data);
                delete[] new_data;
            } else {
                assert(false);
            }
        } else if (pkt->isWrite()) {
            uint64_t config_addr = pkt->getAddr() - MPEG2_BASE_ADDR;

            if (config_addr >= 0x1000000) {
                if (m_enable_dataPlane_opt) {
                    std::memcpy(m_local_node + m_local_node_ptr, pkt->getPtr<uint8_t>(), pkt->getSize());

                    if (m_local_node_ptr == 0) { m_local_node_addr = config_addr; }

                    m_local_node_ptr += pkt->getSize();
                    if (m_local_node_ptr == TASK_DATA_SIZE) {
                        m_data_nodes.push_back(std::make_pair(m_local_node_addr, m_local_node));
                        m_local_node      = new uint8_t[TASK_DATA_SIZE];
                        m_local_node_ptr  = 0;
                        m_local_node_addr = 0;

                        submit();

                    } else if (m_local_node_ptr > TASK_DATA_SIZE) {
                        assert(false);
                    }
                } else {
                    PCIeTask task;
                    task.setValid();
                    task.setWriteType();
                    std::memcpy(&(task.m_content[0]), &config_addr, sizeof(uint64_t));

                    assert(pkt->getSize() == 8);
                    
                    std::memcpy(&(task.m_content[8]), pkt->getPtr<uint8_t>(), pkt->getSize());

                    while (true) {
                        if (m_chimera->recvTask(task) != -1) {
                            break;
                        }
                    }
                }
            } else {
                PCIeTask task;
                task.setValid();
                task.setWriteType();
                std::memcpy(&(task.m_content[0]), &config_addr, sizeof(uint64_t));
                std::memcpy(
                    &(task.m_content[8]), pkt->getPtr<uint8_t>(), pkt->getSize());
                if (config_addr == 0x0 && (task.m_content[8] & 0x2)) {
                    m_stop_signal = task;
                    if (m_local_node_ptr > 0) {
                        m_data_nodes.push_back(std::make_pair(m_local_node_addr, m_local_node));
                        m_local_node      = new uint8_t[TASK_DATA_SIZE];
                        m_local_node_ptr  = 0;
                        m_local_node_addr = 0;
                    }

                    submit();
                } else {
                    if (config_addr == 0x0 && (task.m_content[8] & 0x1)) { m_chimera->enableCDMA(0x10000000, 0); }
                    assert(m_chimera->recvTask(task) != -1);
                }
            }
        } else {
            assert(false);
        }
        pkt->makeTimingResponse();
        if (m_cpu_side_port->sendTimingResp(pkt)) {
        } else {
            m_stalling_packet = pkt;
        }

        m_pending_packets.pop_front();

        if (m_pending_packets.size() > 0 && !m_wakeupEvent->scheduled()) { schedule(m_wakeupEvent, nextCycle()); }
    }
}

void Mpeg2Encoder::Vwakeup()
{
    inputMPEG2  input;
    outputMPEG2 output;
    if (m_pending_packets.size() > 0 && m_stalling_packet == nullptr) {
        PacketPtr pkt = m_pending_packets.front();
        if (pkt->isRead()) {
            uint64_t config_addr = pkt->getAddr() - MPEG2_BASE_ADDR;
            if (config_addr == 0x000000010) {
                uint64_t size = pkt->getSize();
                assert(size == 8);
                uint8_t* new_data = new uint8_t[pkt->getSize()];
                uint64_t value    = 0;
                value |= 0x1;
                value |= (m_last_signal << 2);
                value |= ((m_wptr - m_rptr) << 32);
                std::memcpy(new_data, &value, sizeof(uint64_t));
                pkt->setData(new_data);
                delete[] new_data;
            } else if (config_addr >= 0x010000000) {
                uint8_t* new_data = new uint8_t[pkt->getSize()];

                if (pkt->getSize() <= (m_wptr - m_rptr)) {
                    std::memcpy(new_data, m_buffer + m_rptr, pkt->getSize());
                    m_rptr += pkt->getSize();
                    if (m_wptr == m_rptr) {
                        m_wptr = 0;
                        m_rptr = 0;
                    }
                } else {
                    std::memset(new_data, 0, pkt->getSize());
                }
                pkt->setData(new_data);
                delete[] new_data;
            } else {
                assert(false);
            }
        } else if (pkt->isWrite()) {
            uint64_t config_addr = pkt->getAddr() - MPEG2_BASE_ADDR;
            if (config_addr >= 0x1000000) {
                std::memcpy(m_local_node + m_local_node_ptr, pkt->getPtr<uint8_t>(), pkt->getSize());

                if (m_local_node_ptr == 0) { m_local_node_addr = config_addr; }

                m_local_node_ptr += pkt->getSize();

                input_cnt++;
                if (m_local_node_ptr == 8) {
                    m_local_node_ptr  = 0;
                    m_local_node_addr = 0;

                    input.i_en = 1;
                    input.i_Y0 = m_local_node[0];
                    input.i_U0 = m_local_node[1];
                    input.i_Y1 = m_local_node[2];
                    input.i_V0 = m_local_node[3];
                    input.i_Y2 = m_local_node[4];
                    input.i_U2 = m_local_node[5];
                    input.i_Y3 = m_local_node[6];
                    input.i_V2 = m_local_node[7];

                } else if (m_local_node_ptr > 8) {
                    assert(false);
                }
            } else if (config_addr == 0x0) {
                uint64_t value = 0;
                std::memcpy(&value, pkt->getPtr<uint8_t>(), pkt->getSize());
                input.rstn          = value & 0x1;
                input.sequence_stop = value & 0x2;

                if ((value & 0x1) == 0) {
                    m_last_signal    = false;
                    m_stop_verilator = false;
                }
                if (value & 0x4) { m_stop_verilator = true; }

            } else if (config_addr == 0x8) {
                uint64_t value = 0;
                std::memcpy(&value, pkt->getPtr<uint8_t>(), pkt->getSize());
                input.xsize16 = static_cast<uint32_t>(value >> 32);
                input.ysize16 = static_cast<uint32_t>(value);
            } else {
                assert(false);
            }
        } else {
            assert(false);
        }

        pkt->makeTimingResponse();
        if (m_cpu_side_port->sendTimingResp(pkt)) {
        } else {
            m_stalling_packet = pkt;
        }

        m_pending_packets.pop_front();

        if (m_pending_packets.size() > 0 && !m_VwakeupEvent->scheduled()) { schedule(m_VwakeupEvent, nextCycle()); }
    }

    output = wr->tick(input);

    if (output.o_en) {
        assert(sizeof(output.o_data) == 32);
        std::memcpy(m_buffer + m_wptr, &(output.o_data), sizeof(output.o_data));
        m_wptr += sizeof(output.o_data);

        output_cnt++;
    }

    if (output.o_last) { m_last_signal = true; }

    if (!m_stop_verilator) {
        if (!m_VwakeupEvent->scheduled()) { schedule(m_VwakeupEvent, nextCycle()); }
    }
}


Mpeg2Encoder::Mpeg2EncoderCpuSidePort::Mpeg2EncoderCpuSidePort(const std::string& _name, Mpeg2Encoder* parent) :
    ResponsePort(_name), m_parent(parent)
{
    ranges.push_back(RangeSize(0x100000000, 512 * 1024 * 1024));
}

Mpeg2Encoder::Mpeg2EncoderCpuSidePort::~Mpeg2EncoderCpuSidePort()
{
}

bool Mpeg2Encoder::Mpeg2EncoderCpuSidePort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(Mpeg2Encoder, "received req\n");
    m_parent->m_pending_packets.push_back(pkt);
    if (m_parent->m_enable_verilator) {
        if (!m_parent->m_VwakeupEvent->scheduled()) { m_parent->schedule(m_parent->m_VwakeupEvent, m_parent->nextCycle()); }
    } else {
        if (!m_parent->m_wakeupEvent->scheduled()) { m_parent->schedule(m_parent->m_wakeupEvent, m_parent->nextCycle()); }
    }
    return true;
}

void Mpeg2Encoder::Mpeg2EncoderCpuSidePort::recvRespRetry()
{
    assert(m_parent->m_stalling_packet);
    if (sendTimingResp(m_parent->m_stalling_packet)) {
        m_parent->m_stalling_packet = nullptr;
        if (m_parent->m_pending_packets.size() > 0 && !m_parent->m_wakeupEvent->scheduled()) {
            m_parent->schedule(m_parent->m_wakeupEvent, m_parent->nextCycle());
        }
    } else {
    }
}

Tick Mpeg2Encoder::Mpeg2EncoderCpuSidePort::recvAtomic(PacketPtr pkt)
{
    panic("not implemented\n");
}

Tick Mpeg2Encoder::Mpeg2EncoderCpuSidePort::recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr& backdoor)
{
    panic("not implemented\n");
}

void Mpeg2Encoder::Mpeg2EncoderCpuSidePort::recvFunctional(PacketPtr pkt)
{
    panic("not implemented\n");
}

void Mpeg2Encoder::Mpeg2EncoderCpuSidePort::recvMemBackdoorReq(const MemBackdoorReq& req, MemBackdoorPtr& backdoor)
{
    panic("not implemented\n");
}

AddrRangeList Mpeg2Encoder::Mpeg2EncoderCpuSidePort::getAddrRanges() const
{
    return ranges;
}

} // namespace fpga
} // namespace gem5