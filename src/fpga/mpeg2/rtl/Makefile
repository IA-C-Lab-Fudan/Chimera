DIR_VCD=verilator_mpeg2_vcd
CC=gcc
CXX=g++
OBJS_VERILATE='*.v'

verilate_vcd: $(OBJS)
	$(VERILATOR_ROOT)/bin/verilator --unroll-count 1000 -Wno-lint -cc --trace top.v --Mdir $(DIR_VCD)

library_vcd: verilate_vcd wrapper_mpeg2.cc wrapper_mpeg2.hh
	g++ -I$(DIR_VCD) -I$(VERILATOR_ROOT)/include wrapper_mpeg2.cc $(DIR_VCD)/*.cpp \
	$(VERILATOR_ROOT)/include/verilated.cpp $(VERILATOR_ROOT)/include/verilated_vcd_c.cpp \
	$(VERILATOR_ROOT)/include/verilated_threads.cpp \
	-fpic -shared -o libVerilatorMPEG2.so

install: libVerilatorMPEG2.so
	cp libVerilatorMPEG2.so ../
	cp wrapper_mpeg2.hh ../
	cp rtl_packet_mpeg2.hh ../

.PHONY: clean ultra-clean verilate_vcd library_vcd install

clean:
	rm -rf $(DIR_VCD) $(DIR_FST) libVerilatorMPEG2.so

ultra-clean:
	rm -rf ../libVerilatorMPEG2.so ../wrapper_mpeg2.hh ../rtl_packet_mpeg2.hh