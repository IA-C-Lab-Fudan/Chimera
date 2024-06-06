#ifndef __FPGA_MPEG2_RTL_RTL_PACKET_MPEG2_HH__
#define __FPGA_MPEG2_RTL_RTL_PACKET_MPEG2_HH__

#include <cstdlib>
#include <iostream>

struct inputMPEG2 {
    uint64_t rstn;
    uint64_t xsize16;
    uint64_t ysize16;
    uint64_t i_en;
    uint64_t i_Y0;
    uint64_t i_Y1;
    uint64_t i_Y2;
    uint64_t i_Y3;
    uint64_t i_U0;
    uint64_t i_U1;
    uint64_t i_U2;
    uint64_t i_U3;
    uint64_t i_V0;
    uint64_t i_V1;
    uint64_t i_V2;
    uint64_t i_V3;
    uint64_t sequence_stop;
    uint64_t sequence_busy;

    inputMPEG2()
    {
        rstn          = 1;
        xsize16       = 0;
        ysize16       = 0;
        i_en          = 0;
        i_Y0          = 0;
        i_Y1          = 0;
        i_Y2          = 0;
        i_Y3          = 0;
        i_U0          = 0;
        i_U1          = 0;
        i_U2          = 0;
        i_U3          = 0;
        i_V0          = 0;
        i_V1          = 0;
        i_V2          = 0;
        i_V3          = 0;
        sequence_stop = 0;
        sequence_busy = 0;
    }
};

struct outputMPEG2 {
    uint64_t o_en;
    uint64_t o_last;
    uint32_t o_data[8];
};

#endif