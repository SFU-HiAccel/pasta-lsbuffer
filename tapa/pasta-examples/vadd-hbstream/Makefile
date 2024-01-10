SHELL:=/bin/bash
KERNEL=add
KERNEL_TOP=VecAdd
KERNEL_ARGS=
ifndef platform
platform=xilinx_u280_xdma_201920_3
endif

.PHONY: all cleanall clean cleanxo cleanhdl

all: c xo runxo

c: src/${KERNEL}.cpp src/${KERNEL}-host.cpp
	@echo "[MAKE]: Compiling for C target"
	g++ -o ${KERNEL} -O2 src/${KERNEL}.cpp src/${KERNEL}-host.cpp -I${XILINX_HLS}/include -ltapa -lfrt -lglog -lgflags -lOpenCL -DTAPA_BUFFER_SUPPORT -std=c++17
	./${KERNEL} ${KERNEL_ARGS}

xo: ${KERNEL}
ifndef platform
$(error No platform is set!)
endif
	@echo "[MAKE]: Compiling for XO target"
	tapac -vv -o ${KERNEL}.${platform}.hw.xo src/${KERNEL}.cpp --platform ${platform} --top ${KERNEL_TOP} --work-dir ${KERNEL}.${platform}.hw.xo.tapa --enable-buffer-support --connectivity connectivity.ini

hw: xo
ifndef platform
$(error No platform is set!)
endif
	@echo "[MAKE]: Building HW target"
	source ${KERNEL}.${platform}.hw_generate_bitstream.sh


### INTERMEDIATE OUTPUTS
${KERNEL}: c
${KERNEL}.${platform}.hw.xo: xo


### RUN STUFF
runxo: ${KERNEL}.${platform}.hw.xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Running HW_EMU (.xo)"
	./${KERNEL} ${KERNEL_ARGS} --bitstream=${KERNEL}.${platform}.hw.xo

runxodbg: ${KERNEL}.${platform}.hw.xo
	@echo "[MAKE]: Target waveform"
	@echo "[MAKE]: Running waveform for HW_EMU (.xo)"
	-./${KERNEL} ${KERNEL_ARGS} --bitstream=${KERNEL}.${platform}.hw.xo -xosim_work_dir xosim -xosim_save_waveform
	@head -n -2 xosim/output/run/run_cosim.tcl > xosim/output/run/run_cosim_no_exit.tcl
	@echo "open_wave_config {wave.wcfg}" >> xosim/output/run/run_cosim_no_exit.tcl
	vivado -mode gui -source xosim/output/run/run_cosim_no_exit.tcl

runxov: ${KERNEL}.${platform}.hw.xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Building .xclbin through Vitis"
	export XRT_INI_PATH=scripts/xrt.ini
	v++ -o ${KERNEL}.${platform}.hw_emu.xclbin \
	--link \
	--target hw_emu\
  --kernel ${KERNEL_TOP} \
	--platform ${platform} \
	${KERNEL}.${platform}.hw.xo
	@echo "[MAKE]: Running HW_EMU (.xclbin)"
	./${KERNEL} ${KERNEL_ARGS} --bitstream=${KERNEL}.${platform}.hw_emu.xclbin

runxovdbg: ${KERNEL}.${platform}.hw.xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Building .xclbin through Vitis"
	export XRT_INI_PATH=scripts/xrt.ini
	-v++ \
	-g \
	-o ${KERNEL}.${platform}.hw_emu.xclbin \
	--link \
	--target hw_emu \
  --kernel ${KERNEL_TOP} \
	--platform ${platform} \
	${KERNEL}.${platform}.hw.xo
	@echo "[MAKE]: Running HW_EMU (.xclbin)"
	-./${KERNEL} ${KERNEL_ARGS} --bitstream=${KERNEL}.${platform}.hw_emu.xclbin
	grep -rin "frt.cpp"

runhw: ${KERNEL}.${platform}.hw.xclbin
	@echo "[MAKE]: Target HW"
	@echo "[MAKE]: Running HW (.xclbin)"
	./${KERNEL} --bitstream=${KERNEL}.${platform}.hw.xclbin

### CLEAN
cleanall: clean cleanxo cleandbg cleanhw
	@echo "[MAKE]: Cleaned everything"

cleanhw:
	rm -rf vitis_run_hw

clean:
	rm -f ${KERNEL}

cleanxo:
	rm -f ${KERNEL}.${platform}.hw_emu*
	rm -f ${KERNEL}.${platform}.hw.xo
	rm -f ${KERNEL}.${platform}.hw_generate_bitstream.sh
	rm -f *.log
	rm -rf ${KERNEL}.${platform}.hw.xo.tapa
	rm -rf _x

cleandbg:
	rm -f vivado*.jou
	rm -f vivado*.log
	rm -rf vivado
	rm -rf xosim
	rm -f ${KERNEL}.${platform}.hw.xo
	rm -rf ${KERNEL}.${platform}.hw.xo.tapa

cleanrtl:
	@echo "[MAKE]: Cleaning ${KERNEL}.${platform}.hw.xo.tapa/hdl/"
	rm -rf ${KERNEL}.${platform}.hw.xo.tapa/hdl/
