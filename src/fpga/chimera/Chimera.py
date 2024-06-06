from ast import Param
from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

# FPGAEngine need to be assgined address ranges.

class Chimera(ClockedObject):
    type = 'Chimera'
    cxx_header = "fpga/chimera/chimera.hh"
    cxx_class = 'gem5::fpga::Chimera'

    # When changing this parameter, you need to change two places, and a macro definition needs to be changed, while paying attention to the corresponding parameters in the hardware
    taskTableNum = Param.Int(20, "the outstanding ability of proposed framework")

    batchEnable  = Param.Bool(False, "whether to enable batch optimization")
    batchSize    = Param.Int(3, "supported batch size")

    enableCDMA  = Param.Bool(False, "whether to enable CDMA to poll results from fpga")

    enable_log = Param.Bool(True, "")