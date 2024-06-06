# scl enable devtoolset-10 bash

export VERILATOR_ROOT=/home/data/userhome/fuchao/verilator
export LD_LIBRARY_PATH=/home/data/userhome/fuchao/124-Chimera/src/fpga/mpeg2/rtl:$LD_LIBRARY_PATH

# cd /home/gem5fpga/Desktop/workspace/gem5fpga/chimera

/usr/bin/env python3 $(which scons) build/ARM/gem5.debug PYTHON_CONFIG=/usr/bin/python3-config -j 12
#  --gold-linker