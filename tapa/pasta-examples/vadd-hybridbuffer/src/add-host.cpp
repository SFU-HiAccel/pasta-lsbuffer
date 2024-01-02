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

  std::array<data_type_mmap, N/2> array_a;
  std::array<data_type_mmap, N/2> array_b;
  std::array<data_type_mmap, N/2> array_c_fpga;
  std::array<data_type_mmap, N/2> array_c_cpu;

  srand(0);
  for (int i = 0; i < NDBLKS; i++) {
    for (int j = 0; j < PACK_LENGTH; j++) {  
      array_a[i][j] = PACK_LENGTH*i + j;
      array_b[i][j] = PACK_LENGTH*i + j;
      array_c_cpu[i][j] = array_a[i][j] + array_b[i][j];
      // printf("C[%3d] %3.0f:%3.0f\n", i, array_c_cpu[i][0], array_c_cpu[i][1]);
      // array_a[i] = ((uint64_t)(i*2+1) << 32) | i*2;
      // array_b[i] = ((uint64_t)(i*2+1) << 32) | i*2;
      // // std::cout << (array_a[i] >> 32) << "\t" << (array_a[i] & 0xFFFFFFFF) << std::endl;
      // array_c_cpu[i] = ((array_a[i] + array_b[i]) & 0xFFFFFFFF00000000) | ((array_a[i] + array_b[i]) & 0x00000000FFFFFFFF);
      // // std::cout << (array_c_cpu[i] >> 32) << "\t" << (array_c_cpu[i] & 0xFFFFFFFF) << std::endl;
    }
  }

  const int n_tiles_per_pe = N / TILE;

  int64_t kernel_time_us = tapa::invoke(VecAdd, FLAGS_bitstream,
    tapa::read_only_mmap<const data_type_mmap>(array_a),
    tapa::read_only_mmap<const data_type_mmap>(array_b),
    tapa::write_only_mmap<data_type_mmap>(array_c_fpga), n_tiles_per_pe);

  bool fail = false;
  for (int i = 0; i < N/2; i++) {
    if (array_c_cpu[i][0] != array_c_fpga[i][0] || array_c_cpu[i][1] != array_c_fpga[i][1]) {
      printf("Mismatch [%3d] - Exp %3.0f:%3.0f | Rcv %3.0f:%3.0f\n", i, array_c_cpu[i][0], array_c_cpu[i][1], array_c_fpga[i][0], array_c_fpga[i][1]);
      // printf("Mismatch found at i = %d h:%u l:%u | h:%u l:%u\n", i, array_c_cpu[i] >> 32, array_c_cpu[i] & 0xFFFFFFFF, array_c_fpga[i] >> 32, array_c_fpga[i] & 0xFFFFFFFF);
      // std::cout << (array_c_cpu[i] >> 32) << "\t" << (array_c_cpu[i] & 0xFFFFFFFF) << std::endl;
      // std::cout << "Mismatch found at i = %d %d %d" << i << array_c_cpu[i] << array_c_fpga[i] <<std::endl;
      fail = true;
    }
  }
  if(fail)
    return -1;
  std::cout << "Successfully processed!" << std::endl;
  return 0;
}
