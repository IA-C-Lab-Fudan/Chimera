#ifndef __FPGA_EXAMPLE_MPEG2_ENCODER_HH__
#define __FPGA_EXAMPLE_MPEG2_ENCODER_HH__

#include "params/Mpeg2Encoder.hh"
#include "sim/clocked_object.hh"
#include "mem/packet.hh"
#include "mem/port.hh"
#include "debug/Mpeg2Encoder.hh"

#include "fpga/chimera/chimera.hh"
#include "fpga/mpeg2/rtl/wrapper_mpeg2.hh"

#define MPEG2_BASE_ADDR 0x100000000

namespace gem5
{
namespace fpga
{

class Mpeg2Encoder : public ClockedObject
{
  protected:
    class Mpeg2EncoderCpuSidePort : public ResponsePort
    {
      private:
        Mpeg2Encoder* m_parent;
        AddrRangeList ranges;

      public:
        Mpeg2EncoderCpuSidePort(const std::string& _name, Mpeg2Encoder* parent);
        ~Mpeg2EncoderCpuSidePort();
        bool          recvTimingReq(PacketPtr pkt) override;
        void          recvRespRetry() override;
        Tick          recvAtomic(PacketPtr pkt) override;
        Tick          recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr& backdoor) override;
        void          recvFunctional(PacketPtr pkt) override;
        void          recvMemBackdoorReq(const MemBackdoorReq& req, MemBackdoorPtr& backdoor) override;
        AddrRangeList getAddrRanges() const override;
    };

  private:
    Chimera*                 m_chimera;
    Mpeg2EncoderCpuSidePort* m_cpu_side_port;
    uint8_t                  m_local_data;

    uint8_t*                                 m_local_node;
    uint64_t                                 m_local_node_addr;
    uint64_t                                 m_local_node_ptr;
    std::list<std::pair<uint64_t, uint8_t*>> m_data_nodes;

    PCIeTask m_stop_signal;

    Wrapper_mpeg2* wr;
    uint8_t*       m_buffer;
    uint64_t       m_wptr;
    uint64_t       m_rptr;
    bool           m_last_signal;
    bool           m_stop_verilator;
    bool           m_enable_dataPlane_opt;


  public:
    bool                  m_enable_verilator;
    EventFunctionWrapper* m_wakeupEvent;
    EventFunctionWrapper* m_retryCallbackEvent;
    std::list<PacketPtr>  m_pending_packets;
    PacketPtr             m_stalling_packet;

    EventFunctionWrapper* m_VwakeupEvent;
    void                  Vwakeup();

    Mpeg2Encoder(const Mpeg2EncoderParams& p);
    ~Mpeg2Encoder();
    void  retryCallback();
    void  wakeup();
    void  init() override;
    void  submit();
    Port& getPort(const std::string& if_name, PortID idx = InvalidPortID) override;

    uint64_t input_cnt  = 0;
    uint64_t output_cnt = 0;
};

} // namespace fpga
} // namespace gem5

#endif