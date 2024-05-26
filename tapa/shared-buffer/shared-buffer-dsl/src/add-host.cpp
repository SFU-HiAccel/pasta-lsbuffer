#include <iostream>
#include <vector>
#include <cstdlib>
#include <stdio.h>

#include <gflags/gflags.h>
#include <tapa.h>

#include "add.h"

template <typename T>
using aligned_vector = std::vector<T, tapa::aligned_allocator<T>>;

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  aligned_vector<float> array_a(N);
  aligned_vector<float> array_b(N);
  aligned_vector<float> array_c_cpu(N);
  aligned_vector<float> array_c_fpga(N);

  srand(0);
  for (int i = 0; i < N; i++) {
    array_a[i] = static_cast<float>(i);
    array_b[i] = static_cast<float>(i);
    array_c_cpu[i] = array_a[i] + array_b[i];
  }

  const int n_tiles_per_pe = N / TILE;

  int64_t kernel_time_us = tapa::invoke(VecAdd, FLAGS_bitstream,
    tapa::read_only_mmap<const float>(array_a),
    tapa::read_only_mmap<const float>(array_b),
    tapa::write_only_mmap<float>(array_c_fpga));

  bool fail = false;
  for (int i = 0; i < N; i++) {
    if (array_c_cpu[i] != array_c_fpga[i]) {
      printf("Mismatch [%3d] - Exp %3.0f | Rcv %3.0f\n", i, array_c_cpu[i], array_c_fpga[i]);
      fail = true;
    }
  }
  if(fail)
    return -1;
  std::cout << "Success!" << std::endl;
  return 0;
}
