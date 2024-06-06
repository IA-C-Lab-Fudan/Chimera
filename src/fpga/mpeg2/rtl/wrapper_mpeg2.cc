#include "wrapper_mpeg2.hh"

Wrapper_mpeg2::Wrapper_mpeg2(bool traceOn, std::string name) : tickcount(0), fst(NULL), fstname(name), traceOn(traceOn)
{
    top = new Vtop();

    if (traceOn) {
        Verilated::traceEverOn(traceOn);
        fst = new VerilatedVcdC();
        if (!fst) { return; }

        top->trace(fst, 99);

        std::cout << fstname << std::endl;
        fst->open(fstname.c_str());
    } else {
        fst = nullptr;
    }
}

Wrapper_mpeg2::~Wrapper_mpeg2()
{
    if (fst) {
        fst->dump(tickcount);
        fst->close();
        delete fst;
    }

    top->final();
    delete top;

    exit(EXIT_SUCCESS);
}

void Wrapper_mpeg2::enableTracing()
{
    traceOn = true;
}

void Wrapper_mpeg2::disableTracing()
{
    traceOn = false;
    fst->dump(tickcount);
    fst->close();
}

void Wrapper_mpeg2::tick()
{
    top->clk = 1;
    top->eval();

    advanceTickCount();

    top->clk = 0;
    top->eval();

    advanceTickCount();
}

outputMPEG2 Wrapper_mpeg2::tick(inputMPEG2 in)
{
    processInput(in);

    top->clk = 1;
    top->eval();

    advanceTickCount();

    top->clk = 0;
    top->eval();

    advanceTickCount();

    return processOutput();
}

void Wrapper_mpeg2::processInput(inputMPEG2 in)
{
    top->rstn          = in.rstn;
    top->xsize16       = in.xsize16;
    top->ysize16       = in.ysize16;
    top->i_en          = in.i_en;
    top->i_Y0          = in.i_Y0;
    top->i_Y1          = in.i_Y1;
    top->i_Y2          = in.i_Y2;
    top->i_Y3          = in.i_Y3;
    top->i_U0          = in.i_U0;
    top->i_U1          = in.i_U1;
    top->i_U2          = in.i_U2;
    top->i_U3          = in.i_U3;
    top->i_V0          = in.i_V0;
    top->i_V1          = in.i_V1;
    top->i_V2          = in.i_V2;
    top->i_V3          = in.i_V3;
    top->sequence_stop = in.sequence_stop;
}

outputMPEG2 Wrapper_mpeg2::processOutput()
{
    outputMPEG2 out;

    out.o_en   = top->o_en;
    out.o_last = top->o_last;

    std::memcpy(out.o_data, top->o_data, sizeof(top->o_data));

    return out;
}

void Wrapper_mpeg2::advanceTickCount()
{
    if (fst and traceOn) { fst->dump(tickcount); }
    tickcount++;
}

uint64_t Wrapper_mpeg2::getTickCount()
{
    return tickcount;
}

void Wrapper_mpeg2::reset()
{
    top->rstn = 0;
    top->clk  = 1;
    top->eval();

    advanceTickCount();

    top->clk = 0;
    top->eval();

    advanceTickCount();
    top->rstn = 1;
}