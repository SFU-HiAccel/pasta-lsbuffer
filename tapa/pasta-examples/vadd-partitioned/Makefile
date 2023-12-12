KERNEL=add
KERNEL_TOP=VecAdd
ifndef platform
platform=xilinx_u280_xdma_201920_3
endif

.PHONY: all cleanall clean cleanxo cleanhdl

all: c xo

c: src/${KERNEL}.cpp src/${KERNEL}-host.cpp
	@echo "[MAKE]: Compiling for C target"
	g++ -o ${KERNEL} -O2 src/${KERNEL}.cpp src/${KERNEL}-host.cpp -ltapa -lfrt -lglog -lgflags -lOpenCL -DTAPA_BUFFER_SUPPORT -std=c++17

xo: ${KERNEL}
ifndef platform
$(error No platform is set!)
endif
	@echo "[MAKE]: Compiling for XO target"
	tapac -o ${KERNEL}.${platform}.hw.xo src/${KERNEL}.cpp --platform ${platform} --top ${KERNEL_TOP} --work-dir ${KERNEL}.${platform}.hw.xo.tapa --enable-buffer-support


### INTERMEDIATE OUTPUTS
${KERNEL}: c


### RUN STUFF

runxo: ${KERNEL}.${platform}.hw.xo
	./${KERNEL} --bitstream=${KERNEL}.${platform}.hw.xo



### CLEAN
cleanall: clean cleanxo
	@echo "[MAKE]: CLEANING ALL FILES!"

clean:
	rm -f ${KERNEL}

cleanxo:
	rm -f ${KERNEL}.${platform}.hw.xo
	rm -rf ${KERNEL}.${platform}.hw.xo.tapa

cleanrtl:
	@echo "[MAKE]: Cleaning ${KERNEL}.${platform}.hw.xo.tapa/hdl/"
	rm -rf ${KERNEL}.${platform}.hw.xo.tapa/hdl/