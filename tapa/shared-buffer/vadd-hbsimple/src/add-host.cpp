#include <iostream>
#include <vector>
#include <cstdlib>
#include <stdio.h>

#include <gflags/gflags.h>
#include <tapa.h>

#include "add.h"

void VecAdd(tapa::mmap<const data_type_mmap> vector_a,
            tapa::mmap<const data_type_mmap> vector_b,
            tapa::mmap<data_type_mmap> vector_c, uint64_t n_tiles);

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  std::array<data_type_mmap, N/PACK_LENGTH> array_a;
  std::array<data_type_mmap, N/PACK_LENGTH> array_b;
  std::array<data_type_mmap, N/PACK_LENGTH> array_c_fpga;
  std::array<data_type_mmap, N/PACK_LENGTH> array_c_cpu;

  srand(0);
  for (int i = 0; i < N/PACK_LENGTH; i++) {
    // i = element in array of vectors
    // printf("C[%3d] ", i);
    for (int j = 0; j < PACK_LENGTH; j++) {
      // j = element in vector
      array_a[i][j] = PACK_LENGTH*i + j;
      array_b[i][j] = PACK_LENGTH*i + j;
      array_c_cpu[i][j] = array_a[i][j] + array_b[i][j];
      // printf("%3.0f:", array_c_cpu[i][j]);
    }
    // printf("\n");
  }

  const int n_tiles_per_pe = N / TILE;

  int64_t kernel_time_us = tapa::invoke(VecAdd, FLAGS_bitstream,
    tapa::read_only_mmap<const data_type_mmap>(array_a),
    tapa::read_only_mmap<const data_type_mmap>(array_b),
    tapa::write_only_mmap<data_type_mmap>(array_c_fpga), n_tiles_per_pe);

  bool fail = false;
  for (int i = 0; i < N/PACK_LENGTH; i++) {
    if (array_c_cpu[i][0] != array_c_fpga[i][0] || array_c_cpu[i][1] != array_c_fpga[i][1]) {
      printf("Mismatch [%3d] - Exp %3.0f:%3.0f | Rcv %3.0f:%3.0f\n", i, array_c_cpu[i][0], array_c_cpu[i][1], array_c_fpga[i][0], array_c_fpga[i][1]);
      fail = true;
    }
  }
  if(fail)
    return -1;
  std::cout << "Success!" << std::endl;
  return 0;
}
