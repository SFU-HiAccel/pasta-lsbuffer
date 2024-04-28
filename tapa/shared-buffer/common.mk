SHELL:=/bin/bash

ifndef KERNEL
KERNEL=add
endif

ifndef KERNEL_TOP
KERNEL_TOP=VecAdd
endif

ifndef KERNEL_ARGS
KERNEL_ARGS=
endif

ifndef PLATFORM
PLATFORM=xilinx_u280_gen3x16_xdma_1_202211_1
endif

ifndef XRT_INIT_PATH
XRT_INI_PATH=scripts/xrt.ini
endif

ifndef MAX_SYNTH_JOBS
MAX_SYNTH_JOBS=16
endif

ifndef STRATEGY
STRATEGY="Explore"
endif

ifndef PLACEMENT_STRATEGY
PLACEMENT_STRATEGY="EarlyBlockPlacement"
endif

RUNXOVDBG_OUTPUT_DIR="$$(pwd)/vitis_run_hw_emu"

.PHONY: all c xo runxo runxodbg runxov runxovdbg hw runhw cleanall clean cleanxo cleanhdl

all: c xo hw

################
### SW_EMU
################
c: ${KERNEL}
	-./${KERNEL} ${KERNEL_ARGS}

${KERNEL}: src/${KERNEL}.cpp src/${KERNEL}-host.cpp
	@echo "[MAKE]: Compiling for C target"
	g++ -o ${KERNEL} -O2 src/${KERNEL}.cpp src/${KERNEL}-host.cpp -I${XILINX_HLS}/include -ltapa -lfrt -lglog -lgflags -lOpenCL -DTAPA_BUFFER_SUPPORT -std=c++17


################
### HW_EMU
################
xo: ${KERNEL}.${PLATFORM}.hw.xo

${KERNEL}.${PLATFORM}.hw.xo: ${KERNEL}
ifndef PLATFORM
$(error No PLATFORM is set!)
endif
	@echo "[MAKE]: Compiling for XO target"
	tapac -o ${KERNEL}.${PLATFORM}.hw.xo src/${KERNEL}.cpp --platform ${PLATFORM} --top ${KERNEL_TOP} --work-dir ${KERNEL}.${PLATFORM}.hw.xo.tapa --enable-buffer-support --connectivity connectivity.ini --max-parallel-synth-jobs 24 --separate-complex-buffer-tasks

runxo: xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Running HW_EMU (.xo)"
	./${KERNEL} ${KERNEL_ARGS} --bitstream=${KERNEL}.${PLATFORM}.hw.xo

runxodbg: xo
	@echo "[MAKE]: Target waveform"
	@echo "[MAKE]: Running waveform for HW_EMU (.xo)"
	-./${KERNEL} ${KERNEL_ARGS} --bitstream=${KERNEL}.${PLATFORM}.hw.xo -xosim_work_dir xosim -xosim_save_waveform
	@head -n -2 xosim/output/run/run_cosim.tcl > xosim/output/run/run_cosim_no_exit.tcl
	@echo "open_wave_config {wave.wcfg}" >> xosim/output/run/run_cosim_no_exit.tcl
	vivado -mode gui -source xosim/output/run/run_cosim_no_exit.tcl

runxov: xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Building .xclbin through Vitis"
	v++ -o ${KERNEL}.${PLATFORM}.hw_emu.xclbin \
	--link \
	--target hw_emu\
  --kernel ${KERNEL_TOP} \
	--platform ${PLATFORM} \
	${KERNEL}.${PLATFORM}.hw.xo
	@echo "[MAKE]: Running HW_EMU (.xclbin)"
	./${KERNEL} ${KERNEL_ARGS} --bitstream=${KERNEL}.${PLATFORM}.hw_emu.xclbin

runxovdbg: xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Building .xclbin through Vitis"
	export XRT_INI_PATH=scripts/xrt.ini
	v++ -g \
	--link \
	--output "${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_TOP}.${PLATFORM}.xclbin" \
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
	-./${KERNEL} ${KERNEL_ARGS} --bitstream=${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_TOP}.${PLATFORM}.xclbin
	xsim -gui *.wdb



################
### HARDWARE
################
hw: ${KERNEL}.${PLATFORM}.hw.xclbin

${KERNEL}.${PLATFORM}.hw.xclbin: ${KERNEL}.${PLATFORM}.hw.xo
ifndef PLATFORM
$(error No PLATFORM is set!)
endif
	@echo "[MAKE]: Building HW target"
	source ${KERNEL}.${PLATFORM}.hw_generate_bitstream.sh

runhw: hw
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
# delete all logs
	rm -f *.jou
	rm -f *.log
	rm -f *.run_summary
# delete bloat
	rm -rf vivado
	rm -rf xosim
	rm -rf vitis_run_hw_emu
	rm -rf *.protoinst
	rm -rf xsim.dir
# only delete wave configurations that were automatically generated	
	rm -rf *${PLATFORM}*.wcfg
	rm -f ${KERNEL}.${PLATFORM}.hw.xo
	rm -rf ${KERNEL}.${PLATFORM}.hw.xo.tapa
# delete some intermediary files generated from the custom scripts
	rm -f opencl_trace.csv profile_kernels.csv summary.csv timeline_kernels.csv runs.md

cleanrtl:
	@echo "[MAKE]: Cleaning ${KERNEL}.${PLATFORM}.hw.xo.tapa/hdl/"
	rm -rf ${KERNEL}.${PLATFORM}.hw.xo.tapa/hdl/
