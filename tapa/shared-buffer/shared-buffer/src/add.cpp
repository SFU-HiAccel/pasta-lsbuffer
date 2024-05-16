#include <tapa.h>
#include "add.h"
#include "brahma.h"

using sb_t = SharedBuffer<sb_msg_t, SB_NUM_PAGES, SB_NRX, SB_NTX, (SB_NRX+SB_NTX)>;

void vadd(tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer_a,
          tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer_b,
          tapa::obuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer_c,
          int n_tiles) {
  for (int tile_id = 0; tile_id < n_tiles; tile_id++) {
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
}

//////////////////
/// LOAD
//////////////////


void load(tapa::mmap<bits<data_type_mmap>> argmmap,
          tapa::obuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer_load,
          int n_tiles) {
  for (int tile_id = 0; tile_id < n_tiles; tile_id++) {
    #pragma HLS pipeline off
    auto section = buffer_load.acquire();
    auto& buf_ref = section();
    data_type temp;
    for (int j = 0; j < TILE/PACK_LENGTH; j++) {
      #pragma HLS pipeline II=1
      // #pragma HLS unroll factor=2
      auto packvec = tapa::bit_cast<data_type_mmap>(argmmap[tile_id*TILE/(PACK_LENGTH) + j]);
      buf_ref[2*j] = float(packvec[0]);
      buf_ref[2*j+1] = float(packvec[1]);
    }
  }
}

//////////////////
/// STORE
//////////////////
void store(tapa::mmap<bits<data_type_mmap>> argmmap,
           tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer_store,
           int n_tiles) {
  for (int tile_id = 0; tile_id < n_tiles; tile_id++) {
#pragma HLS pipeline off
    auto section = buffer_store.acquire();
    auto& buf_ref = section();
    for (int j = 0; j < TILE/PACK_LENGTH; j++) {
#pragma HLS pipeline II=1
      data_type_mmap packvec;
      packvec[0] = buf_ref[2*j];
      packvec[1] = buf_ref[2*j+1];
      argmmap[tile_id*TILE/(PACK_LENGTH) + j] = tapa::bit_cast<bits<data_type_mmap>>(packvec);
    }
  }
}

void task1( tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer,
            sb_t sbo,
            tapa::ostream<sb_msg_t>& tx_task1_to_sb,
            tapa::istream<sb_msg_t>& rx_sb_to_task1,
            tapa::ostream<sb_pageid_t>& tx_task1_to_task2)
{
  sb_pageid_t allocated_page1 = sbo.request_page(tx_task1_to_sb, 1);
  
}

void task2( tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& buffer,
            sb_t sbo,
            tapa::ostream<sb_msg_t>& tx_task2_to_sb,
            tapa::istream<sb_msg_t>& rx_sb_to_task2,
            tapa::istream<sb_pageid_t>& rx_task1_to_task2)
{
  
}

////////////////
/// WRAPPER
////////////////

void VecAdd(tapa::mmap<bits<data_type_mmap>> vector_a,
            tapa::mmap<bits<data_type_mmap>> vector_b,
            tapa::mmap<bits<data_type_mmap>> vector_c,
            uint64_t n_tiles) {

  sb_t sbo;
  // std::cout << sbo.get_iport(1) << std::endl;
  std::cout << "===" << std::endl;
  tapa::buffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>> buffer_a;
  tapa::buffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>> buffer_b;
  tapa::buffer<data_type[TILE], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>> buffer_c;
  
  
  tapa::task()
    .invoke(load, vector_a, buffer_a, n_tiles)
    .invoke(load, vector_b, buffer_b, n_tiles)
    .invoke(task1, buffer_a, sbo, sbo.get_iport(0), sbo.get_oport(0))
    .invoke(task2, buffer_b, sbo, sbo.get_iport(1), sbo.get_oport(1))
    // .invoke(readFromBuffer, buffer_c, sbif_task3)
    .invoke(store, vector_c, buffer_c, n_tiles);
}
