#ifndef __FPGA_MPEG2_RTL_WRAPER_MPEG2_HH__
#define __FPGA_MPEG2_RTL_WRAPER_MPEG2_HH__

#include <cstdlib>
#include <iostream>

#include "Vtop.h"

#include "verilated.h"
#include "verilated_vcd_c.h"

#include "rtl_packet_mpeg2.hh"

class Wrapper_mpeg2
{
  public:
    Wrapper_mpeg2(bool traceOn, std::string name);
    ~Wrapper_mpeg2();

    void        tick();
    outputMPEG2 tick(inputMPEG2 in);
    uint64_t    getTickCount();
    void        enableTracing();
    void        disableTracing();
    void        advanceTickCount();
    void        reset();
    void        processInput(inputMPEG2 in);
    outputMPEG2 processOutput();

  private:
    Vtop*          top;
    uint64_t       tickcount;
    VerilatedVcdC* fst;
    std::string    fstname;
    bool           traceOn;
};

#endif // !__FPGA_MPEG2_RTL_WRAPER_MPEG2_HH__
