- [What is Chimera?](#what-is-chimera)
- [**Prerequisites**](#prerequisites)
- [Run the example project using Chimera](#run-the-example-project-using-chimera)
- [Verify your own design using Chimera](#verify-your-own-design-using-chimera)
- [License](#license)
- [Acknowledgments](#acknowledgments)


# What is Chimera?

Chimera is a comprehensive co-simulation framework that combines the gem5 simulator and FPGA platforms. The framework streamlines the conventional development process, providing a direct approach to architecture exploration and integration verification of a Register-Transfer Level (RTL) IP within an SoC running on the gem5 simulator.

Chimera is published in FPL 2024.

> Chao Fu, Zengshi Wang, Jun Han. "Chimera: A co-simulation framework combining with gem5 and FPGA platform for efficient verification." *2024 34rd International Conference on Field-Programmable Logic and Applications (FPL)*. IEEE, 2024.

# **Prerequisites**

1. Clone the repository from Github

```bash
git clone https://github.com/IA-C-Lab-Fudan/Chimera
```

1. Install Verilator (Verilator is installed to support the [gem5+rtl project](https://gitlab.bsc.es/glopez/gem5-rtl), which is also included in the Chimera project)

```bash
git clone https://github.com/verilator/verilator

autoconf
./configure --prefix <you path to install Verilator>

make -j 6
make install

export VERILATOR_ROOT=<you path to install Verilator>
```

1. Generate the bitstream using Vivado

You need to select an FPGA board that supports PCIe functionality and build a project using Vivado to generate bitstream from the hardware code we provided and deploy it to the FPGA board. The hardware code is located at **<src/fpga/verilog>**.

After connecting the PCIe interface of the FPGA board to the host, restart Linux and run **lspci** to check whether the FPGA board (which is treated as a PCIe device) is recognized properly. If you find "Memory controller: Xilinx..." is displayed, the identification is successful.

1. Install Xilinx XDMA driver

```bash
git clone https://github.com/Xilinx/dma_ip_drivers

cd XDMA/linux-kernel/xdma
make install

cd XDMA/linux-kernel/tests
./load_driver.sh
```

You can see "DONE" if the driver is successfully loaded.

1. Build the RTL C++ model using Verilator

```bash
cd src/fpga/mpeg2/rtl

make
make library_vcd
make install
```

1. Config compilation path for gem5 simulator

```bash
# file: Sconstruct, you need fill in the following paths
main.Append(CPPPATH=[Dir('<path to Chimera project>/src/fpga/mpeg2/rtl')])
main.Append(CPPPATH=[Dir('<path to Chimera project>/src/fpga/mpeg2/rtl/verilator_mpeg2_vcd')])

main.Prepend(LIBPATH=[Dir('<path to Chimera project>/src/fpga/mpeg2/rtl')])
```

1. Compile the Chimera project

Chimera is based on the gem5 project, and compiling gem5 has the following dependencies:

- **git** : gem5 uses git for version control.
- **gcc**: gcc is used to compiled gem5. **Version >=8 must be used**. We support up to gcc Version 12. **Note**: GCC Version 9 may be used but is not officially supported due to it [increasing gem5 Object File sizes](https://github.com/gem5/gem5/issues/555).
- **Clang**: Clang can also be used. At present, we support Clang 7 to Clang 16 (inclusive).
- **SCons** : gem5 uses SCons as its build environment. SCons 3.0 or greater must be used.
- **Python 3.6+** : gem5 relies on Python development libraries. gem5 can be compiled and run in environments using Python 3.6+.
- **protobuf 2.1+** (Optional): The protobuf library is used for trace generation and playback.
- **Boost** (Optional): The Boost library is a set of general purpose C++ libraries. It is a necessary dependency if you wish to use the SystemC implementation.

If you ensure that the above dependencies are met, then you can compile:

```bash
scons build/ARM/gem5.opt -j 12
```

# Run the example project using Chimera

1. Config environment path of link library

```bash
export LD_LIBRARY_PATH=<path to Chimera project>/src/fpga/mpeg2/rtl:$LD_LIBRARY_PATH
```

1. Run gem5 with Chimera (example command)

```bash
build/ARM/gem5.opt configs/example/se.py \
--caches \
--num-cpus=1 \
--cpu-type=TimingSimpleCPU \
--mem-size=4GB \
--enable-cosim \
--enable-mpeg2 \
--cmd=<path to Chimera project>/tests/test-progs/phy/phy \
--options=<path to Chimera project>/tests/test-progs/phy/288x208.raw <path to Chimera project>/tests/test-progs/phy/288x208.m2v
```

1. Configurable parameters that Chimera supports

- **enable-cosim:** whether to enable the co-simulation
  - **enable-chimera:** whether to enable the Chimera (Takes effect only if **enable-cosim** is valid)
  - **enable-verilator:** whether to enable th gem5+rtl framework (Takes effect only if **enable-cosim** is valid)
- **enable-mpeg2:** whether to enable MPEG2 encoder accelerator during co-simulation
- **chimera-table-num:** outstanding ability of Chimera
- **enable-sync-opt:** whether to enable synchronization optimization
- **enable-dump-wave:** whether to dump wave file during co-simulation with Verilator

# Verify your own design using Chimera

You can co-simulate your own modules with Chimera in just **3 steps**. Of course, you need to prepare the code related to your module in the SoC in advance, as well as the hardware implementation of the module you develop.

1. Analyze the decoupling between modules and SOCs. (Chimera currently only guarantees coarse-grained modules with superior co-simulation performance)
2. Specifies the interaction interface between the SoC and the modules. You need to specify this in the **<src/fpga/chimera/common.hh>** file.
3. Develop the hardware interface code between the hardware implementation of your modules  with Chimera framework. You can refer to the **<src/fpga/verilog/ModuleIFS_Mpeg2.v>** file. This module configures the input of the module with the received signal packet and packages the output of the module as a signal packet.

# License

Distributed under the [MIT](https://choosealicense.com/licenses/mit/) License.

# Acknowledgments

1. This work was supported by the National Natural Science Foundation of China under Grant 61934002.
2. Parts of this tutorial refer to [gem5+rtl](https://gitlab.bsc.es/glopez/gem5-rtl)
3. Parts of this tutorial refer to [Xilinx-FPGA-PCIe-XDMA-Tutorial](https://github.com/ssuperfu/Xilinx-FPGA-PCIe-XDMA-Tutorial)