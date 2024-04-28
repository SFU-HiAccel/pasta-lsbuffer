#include <tapa.h>
#include <buffer.h>
#include "add.h"
#include "sb.h"

void vadd(tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer_a,
          tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer_b,
          tapa::obuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer_c) {
  #pragma HLS pipeline off
  auto section_a = buffer_a.acquire();
  auto section_b = buffer_b.acquire();
  auto section_c = buffer_c.acquire();

  auto& buf_rf_a = section_a();
  auto& buf_rf_b = section_b();
  auto& buf_rf_c = section_c();

COMPUTE_LOOP:
  for (int j = 0; j < TILE; j++) {
    #pragma HLS pipeline II=1
    #pragma HLS unroll factor=2
    buf_rf_c[j] = buf_rf_a[j] + buf_rf_b[j];
  }
}

//////////////////
/// LOAD
//////////////////


void load(tapa::mmap<const data_type_mmap> argmmap,
          tapa::obuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer_load) {
  #pragma HLS pipeline off
  auto section = buffer_load.acquire();
  auto& buf_ref = section();
  data_type temp;
  for (int j = 0; j < TILE/PACK_LENGTH; j++) {
    #pragma HLS pipeline II=1
    // #pragma HLS unroll factor=2
    data_type_mmap packvec = argmmap[tile_id*TILE/(PACK_LENGTH) + j];
    buf_ref[2*j] = packvec[0];
    buf_ref[2*j+1] = packvec[1];
  }
}

//////////////////
/// STORE
//////////////////
void store(tapa::mmap<data_type_mmap> argmmap,
           tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer_store,
           int n_tiles) {
  #pragma HLS pipeline off
  auto section = buffer_store.acquire();
  auto& buf_ref = section();
  for (int j = 0; j < TILE/PACK_LENGTH; j++) {
    #pragma HLS pipeline II=1
    data_type_mmap packvec;
    packvec[0] = buf_ref[2*j];
    packvec[1] = buf_ref[2*j+1];
    argmmap[tile_id*TILE/(PACK_LENGTH) + j] = packvec;
  }
}

////////////////
/// WRAPPER
////////////////

void VecAdd(tapa::mmap<const data_type_mmap> vector_a,
            tapa::mmap<const data_type_mmap> vector_b,
            tapa::mmap<data_type_mmap> vector_c, uint64_t n_tiles) {
  tapa::buffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>> buffer_a;
  tapa::buffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>> buffer_b;
  tapa::buffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>> buffer_c;
  tapa::stream<data_type> a_q0("a0");
  tapa::stream<data_type> a_q1("a1");
  tapa::stream<data_type> b_q0("b0");
  tapa::stream<data_type> b_q1("b1");
  tapa::stream<data_type> c_q0("c0");
  tapa::stream<data_type> c_q1("c1");
  SharedBuffer<data_type_mmap, NPROD, NCONS, TILE, (NPROD+NCONS)> buffer;
  std::cout << sb_dt.get_container_size() << std::endl;
  tapa::task()
    .invoke(load, vector_a, buffer_a, n_tiles)
    .invoke(load, vector_b, buffer_b, n_tiles)
    .invoke(vadd, buffer_a, buffer_b, buffer_c, n_tiles)
    .invoke(store, vector_c, buffer_c, n_tiles);
}
