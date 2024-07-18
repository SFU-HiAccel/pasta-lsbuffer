#!/bin/sh

g++ -g -o vadd -O2 src/add.cpp src/add-host.cpp -ltapa -lfrt -lglog -lgflags -lOpenCL -std=c++17 -DTAPA_BUFFER_SUPPORT -DTAPA_EXPLICIT_BUFFER_RELEASE
./vadd
