SHELL:=/bin/bash
KERNEL=add
KERNEL_TOP=VecAdd
KERNEL_ARGS=
ifndef PLATFORM
PLATFORM=xilinx_u280_xdma_201920_3
endif

XRT_INI_PATH=scripts/xrt.ini
MAX_SYNTH_JOBS=24
STRATEGY="Explore"
PLACEMENT_STRATEGY="EarlyBlockPlacement"
RUNXOVDBG_OUTPUT_DIR="$$(pwd)/vitis_run_hw_emu"

.PHONY: all cleanall clean cleanxo cleanhdl

all: c xo runxo

### INTERMEDIATE OUTPUTS
${KERNEL}: c
${KERNEL}.${PLATFORM}.hw.xo: xo


c: src/${KERNEL}.cpp src/${KERNEL}-host.cpp
	@echo "[MAKE]: Compiling for C target"
	g++ -o ${KERNEL} -O2 src/${KERNEL}.cpp src/${KERNEL}-host.cpp -I${XILINX_HLS}/include -ltapa -lfrt -lglog -lgflags -lOpenCL -DTAPA_BUFFER_SUPPORT -std=c++17
	-./${KERNEL} ${KERNEL_ARGS}

xo: ${KERNEL}
ifndef PLATFORM
$(error No PLATFORM is set!)
endif
	@echo "[MAKE]: Compiling for XO target"
	tapac -o ${KERNEL}.${PLATFORM}.hw.xo src/${KERNEL}.cpp --platform ${PLATFORM} --top ${KERNEL_TOP} --work-dir ${KERNEL}.${PLATFORM}.hw.xo.tapa --enable-buffer-support --connectivity connectivity.ini

hw: xo
ifndef PLATFORM
$(error No PLATFORM is set!)
endif
	@echo "[MAKE]: Building HW target"
	source ${KERNEL}.${PLATFORM}.hw_generate_bitstream.sh


### RUN STUFF
runxo: ${KERNEL}.${PLATFORM}.hw.xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Running HW_EMU (.xo)"
	./${KERNEL} ${KERNEL_ARGS} --bitstream=${KERNEL}.${PLATFORM}.hw.xo

runxodbg: ${KERNEL}.${PLATFORM}.hw.xo
	@echo "[MAKE]: Target waveform"
	@echo "[MAKE]: Running waveform for HW_EMU (.xo)"
	-./${KERNEL} ${KERNEL_ARGS} --bitstream=${KERNEL}.${PLATFORM}.hw.xo -xosim_work_dir xosim -xosim_save_waveform
	@head -n -2 xosim/output/run/run_cosim.tcl > xosim/output/run/run_cosim_no_exit.tcl
	@echo "open_wave_config {wave.wcfg}" >> xosim/output/run/run_cosim_no_exit.tcl
	vivado -mode gui -source xosim/output/run/run_cosim_no_exit.tcl

runxov: ${KERNEL}.${PLATFORM}.hw.xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Building .xclbin through Vitis"
	export XRT_INI_PATH=scripts/xrt.ini
	v++ -o ${KERNEL}.${PLATFORM}.hw_emu.xclbin \
	--link \
	--target hw_emu\
  --kernel ${KERNEL_TOP} \
	--PLATFORM ${PLATFORM} \
	${KERNEL}.${PLATFORM}.hw.xo
	@echo "[MAKE]: Running HW_EMU (.xclbin)"
	./${KERNEL} ${KERNEL_ARGS} --bitstream=${KERNEL}.${PLATFORM}.hw_emu.xclbin

runxovdbg: ${KERNEL}.${PLATFORM}.hw.xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Building .xclbin through Vitis"
	v++ -g \
  --link \
  --output "${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_TOP}_${PLATFORM}.xclbin" \
  --kernel ${KERNEL_TOP} \
  --platform ${PLATFORM} \
  --target hw_emu \
  --report_level 2 \
  --temp_dir "${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_TOP}_${PLATFORM}.temp" \
  --optimize 3 \
  --connectivity.nk ${KERNEL_TOP}:1:${KERNEL_TOP} \
  --save-temps \
  "${KERNEL}.${PLATFORM}.hw.xo" \
  --vivado.synth.jobs ${MAX_SYNTH_JOBS} \
  --vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.IS_ENABLED=1 \
  --vivado.prop=run.impl_1.STEPS.OPT_DESIGN.ARGS.DIRECTIVE=Explore \
  --vivado.prop=run.impl_1.STEPS.PLACE_DESIGN.ARGS.DIRECTIVE=EarlyBlockPlacement \
  --vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE=Explore \
  --vivado.prop=run.impl_1.STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE=Explore \
  --config "connectivity.ini"
	@echo "[MAKE]: Running HW_EMU (.xclbin)"
	-./${KERNEL} ${KERNEL_ARGS} --bitstream="${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_TOP}.${PLATFORM}.xclbin"

runhw: ${KERNEL}.${PLATFORM}.hw.xclbin
	@echo "[MAKE]: Target HW"
	@echo "[MAKE]: Running HW (.xclbin)"
	./${KERNEL} ${KERNEL_ARGS} --bitstream=vitis_run_hw/${KERNEL_TOP}.${PLATFORM}.hw.xclbin

### CLEAN
cleanall: clean cleanxo cleandbg cleanhw
	@echo "[MAKE]: Cleaned everything"

cleanhw:
	rm -rf vitis_run_hw

clean:
	rm -f ${KERNEL}

cleanxo:
	rm -f ${KERNEL}.${PLATFORM}.hw_emu*
	rm -f ${KERNEL}.${PLATFORM}.hw.xo
	rm -f ${KERNEL}.${PLATFORM}.hw_generate_bitstream.sh
	rm -f *.log
	rm -rf ${KERNEL}.${PLATFORM}.hw.xo.tapa
	rm -rf _x

cleandbg:
	rm -f vivado*.jou
	rm -f vivado*.log
	rm -rf vivado
	rm -rf xosim
	rm -f ${KERNEL}.${PLATFORM}.hw.xo
	rm -rf ${KERNEL}.${PLATFORM}.hw.xo.tapa

cleanrtl:
	@echo "[MAKE]: Cleaning ${KERNEL}.${PLATFORM}.hw.xo.tapa/hdl/"
	rm -rf ${KERNEL}.${PLATFORM}.hw.xo.tapa/hdl/
