from ast import Param
from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject
from m5.objects.Chimera import Chimera

# FPGAEngine need to be assgined address ranges.

class Mpeg2Encoder(ClockedObject):
    type = 'Mpeg2Encoder'
    cxx_header = "fpga/mpeg2/mpeg2_encoder.hh"
    cxx_class = 'gem5::fpga::Mpeg2Encoder'

    chimera = Param.Chimera("pcie engine")
    enable_verilator = Param.Bool(False, "whether to enable verilator simulation")
    dump_wave = Param.Bool(False, "whether to dump wave from verilator")

    cpu_side_port = ResponsePort(
        "This port receives requests and sends responses"
    )

    enable_dataPlane_opt = Param.Bool(True, "")