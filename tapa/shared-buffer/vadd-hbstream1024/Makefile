SHELL:=/bin/bash
#KERNEL=add
#KERNEL_TOP=VecAdd
#KERNEL_ARGS=
PLATFORM=xilinx_u280_gen3x16_xdma_1_202211_1
BUILD_DIR_PREFIX=${SIM_WS}/builds/sb/vadd-hbstream1024/
# BUILD_DIR_PREFIX=builds

#XRT_INI_PATH=scripts/xrt.ini
MAX_SYNTH_JOBS=24
#STRATEGY="Explore"
#PLACEMENT_STRATEGY="EarlyBlockPlacement"
XO_FLAGS=--floorplan-pre-assignments manual_1.json --floorplan-output ${BUILD_DIR_PREFIX}/constraint.tcl
#XO_FLAGS=--floorplan-output ${BUILD_DIR_PREFIX}/constraint.tcl

include ../common.mk
