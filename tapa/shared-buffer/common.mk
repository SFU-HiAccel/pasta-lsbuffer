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

KERNEL_CO=${BUILD_DIR_PREFIX}/${KERNEL}
KERNEL_XO="${BUILD_DIR_PREFIX}/${KERNEL}.${PLATFORM}.hw.xo"
KERNEL_XCLBIN_EM="${BUILD_DIR_PREFIX}/vitis_run_hw_emu/${KERNEL}.${PLATFORM}.hw_emu.xclbin"
KERNEL_XCLBIN_HW="${BUILD_DIR_PREFIX}/vitis_run_hw/${KERNEL}.${PLATFORM}.hw.xclbin"
RUNXOVDBG_OUTPUT_DIR="${BUILD_DIR_PREFIX}/vitis_run_hw_emu"
VPP_TEMP_DIR="${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_TOP}_${PLATFORM}.temp"


.PHONY: all c xo runxo runxodbg runxov runxovdbg hw runhw cleanall clean cleanxo cleanhdl

all: c xo hw

################
### SW_EMU
################
c: ${KERNEL_CO}
	-${KERNEL_CO} ${KERNEL_ARGS}

${KERNEL_CO}: src/${KERNEL}.cpp src/${KERNEL}-host.cpp
	@echo "[MAKE]: Compiling for C target"
	g++ -g -o ${KERNEL_CO} -O2 src/${KERNEL}.cpp src/${KERNEL}-host.cpp -I${XILINX_HLS}/include -ltapa -lfrt -lglog -lgflags -lOpenCL -DTAPA_BUFFER_SUPPORT -std=c++17


################
### HW_EMU
################

xo: ${KERNEL_XO}
	@echo "[MAKE]: Looking for prebuilt XO"

${KERNEL_XO}: ${KERNEL_CO}
	@echo "[MAKE]: Compiling for XO target"
	tapac -vv -o ${KERNEL_XO} src/${KERNEL}.cpp --platform ${PLATFORM} --top ${KERNEL_TOP} --work-dir ${KERNEL_XO}.tapa --enable-buffer-support --connectivity connectivity.ini --max-parallel-synth-jobs 24 --separate-complex-buffer-tasks

runxo: ${KERNEL_XO}
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Running HW_EMU (.xo)"
	@cd ${BUILD_DIR_PREFIX}
	${KERNEL_CO} ${KERNEL_ARGS} --bitstream=${KERNEL_XO}

runxodbg: ${KERNEL_XO}
	@echo "[MAKE]: Target waveform"
	@echo "[MAKE]: Running waveform for HW_EMU (.xo)"
	@cd ${BUILD_DIR_PREFIX}
	-${KERNEL_CO} ${KERNEL_ARGS} --bitstream=${KERNEL_XO} -xosim_work_dir ${BUILD_DIR_PREFIX}/xosim -xosim_save_waveform
	@head -n -2 ${BUILD_DIR_PREFIX}/xosim/output/run/run_cosim.tcl > ${BUILD_DIR_PREFIX}/xosim/output/run/run_cosim_no_exit.tcl
	@echo "open_wave_config {${BUILD_DIR_PREFIX}/wave.wcfg}" >> ${BUILD_DIR_PREFIX}/xosim/output/run/run_cosim_no_exit.tcl
	vivado -mode gui -source ${BUILD_DIR_PREFIX}/xosim/output/run/run_cosim_no_exit.tcl

runxov: ${KERNEL_XO}
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Building .xclbin through Vitis"
	@cd ${BUILD_DIR_PREFIX}
	v++ -o ${KERNEL_XCLBIN_EM} \
	--link \
	--target hw_emu\
  --kernel ${KERNEL_TOP} \
	--platform ${PLATFORM} \
	--temp_dir ${VPP_TEMP_DIR} \
	--log_dir ${VPP_TEMP_DIR}/logs \
	--report_dir ${VPP_TEMP_DIR}/reports \
	${KERNEL_XO}
	@echo "[MAKE]: Running HW_EMU (.xclbin)"
	@cd ${BUILD_DIR_PREFIX}
	${KERNEL_CO} ${KERNEL_ARGS} --bitstream=${KERNEL_XCLBIN_EM}

runxovdbg: ${KERNEL_XO}
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Building .xclbin through Vitis"
	export XRT_INI_PATH=${PWD}/scripts/xrt.ini
	@cd ${BUILD_DIR_PREFIX}
	v++ -g \
	--link \
	--output "${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_XCLBIN_EM}" \
	--kernel ${KERNEL_TOP} \
	--platform ${PLATFORM} \
	--target hw_emu \
	--report_level 2 \
	--temp_dir ${VPP_TEMP_DIR} \
	--log_dir ${VPP_TEMP_DIR}/logs \
	--report_dir ${VPP_TEMP_DIR}/reports \
	--optimize 3 \
	--connectivity.nk ${KERNEL_TOP}:1:${KERNEL_TOP} \
	--save-temps \
	"${KERNEL_XO}" \
	--vivado.synth.jobs ${MAX_SYNTH_JOBS} \
	--vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.IS_ENABLED=1 \
	--vivado.prop=run.impl_1.STEPS.OPT_DESIGN.ARGS.DIRECTIVE=Explore \
	--vivado.prop=run.impl_1.STEPS.PLACE_DESIGN.ARGS.DIRECTIVE=EarlyBlockPlacement \
	--vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE=Explore \
	--vivado.prop=run.impl_1.STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE=Explore \
	--config "connectivity.ini"
	@echo "[MAKE]: Running HW_EMU (.xclbin)"
	-${KERNEL_CO} ${KERNEL_ARGS} --bitstream=${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_XCLBIN_EM}
	xsim -gui *.wdb



################
### HARDWARE
################
hw: ${KERNEL_XCLBIN_HW}

${KERNEL_XCLBIN_HW}: xo
	@echo "[MAKE]: Building HW target"
	cd ${BUILD_DIR_PREFIX} && source ${BUILD_DIR_PREFIX}/${KERNEL}.${PLATFORM}.hw_generate_bitstream.sh

runhw: ${KERNEL_XCLBIN_HW}
	@echo "[MAKE]: Target HW"
	@echo "[MAKE]: Running HW (.xclbin)"
	cd ${BUILD_DIR_PREFIX} && ${KERNEL_CO} ${KERNEL_ARGS} --bitstream=${BUILD_DIR_PREFIX}/vitis_run_hw/${KERNEL_XCLBIN_HW}

### CLEAN
cleanall: clean cleanxo cleandbg cleanhw
	@echo "[MAKE]: Cleaned everything"

cleanhw:
	rm -rf vitis_run_hw

clean:
	rm -f ${KERNEL_CO}

cleanxo:
	rm -f ${KERNEL_XCLBIN_EM}
	rm -f ${KERNEL_XO}
	rm -f ${KERNEL}.${PLATFORM}.hw_generate_bitstream.sh
	rm -rf ${KERNEL_XO}.tapa

cleandbg:
# delete all logs
	rm -f ${BUILD_DIR_PREFIX}/*.jou
	rm -f ${BUILD_DIR_PREFIX}/*.log
	rm -f ${BUILD_DIR_PREFIX}/*.run_summary
# delete bloat
	rm -rf ${BUILD_DIR_PREFIX}/vivado
	rm -rf ${BUILD_DIR_PREFIX}/xosim
	rm -rf ${BUILD_DIR_PREFIX}/vitis_run_hw_emu
	rm -rf ${BUILD_DIR_PREFIX}/*.protoinst
	rm -rf ${BUILD_DIR_PREFIX}/xsim.dir
# only delete wave configurations that were automatically generated	
	rm -rf ${BUILD_DIR_PREFIX}/*${PLATFORM}*.wcfg
	rm -f  ${KERNELXO}
	rm -rf ${KERNEL_XO}.tapa
# delete some intermediary files generated from the custom scripts
	cd ${BUILD_DIR_PREFIX}
	rm -f  opencl_trace.csv profile_kernels.csv summary.csv timeline_kernels.csv runs.md

cleanrtl:
	@echo "[MAKE]: Cleaning ${KERNEL_XO}.tapa/hdl/"
	rm -rf ${KERNEL_XO}.tapa/hdl/
