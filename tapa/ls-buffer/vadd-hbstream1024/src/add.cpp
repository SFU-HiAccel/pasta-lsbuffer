#include <tapa.h>

#include "add.h"


void vadd(tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::cyclic<2>>, tapa::memcore<tapa::bram>>& buffer_a,
          tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::cyclic<2>>, tapa::memcore<tapa::bram>>& buffer_b,
          tapa::obuffer<data_type[TILE], 1, tapa::array_partition<tapa::cyclic<2>>, tapa::memcore<tapa::bram>>& buffer_c,
          int n_tiles) {
  for (int tile_id = 0; tile_id < n_tiles; tile_id++) {
#pragma HLS pipeline off

    auto section_a = buffer_a.create_section();
    auto section_b = buffer_b.create_section();
    auto section_c = buffer_c.create_section();
    buffer_a.acquire(section_a);
    buffer_b.acquire(section_b);
    buffer_c.acquire(section_c);

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


void load(tapa::mmap<const data_type_mmap> argmmap,
          tapa::obuffer<data_type[TILE], 1, tapa::array_partition<tapa::cyclic<2>>, tapa::memcore<tapa::bram>>& buffer_load,
          int n_tiles) {
  for (int tile_id = 0; tile_id < n_tiles; tile_id++) {
    #pragma HLS pipeline off
    auto section = buffer_load.create_section();
    buffer_load.acquire(section);
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
}

void loadStream(tapa::istream<data_type>& a0,
          tapa::istream<data_type>& a1,
          tapa::obuffer<data_type[TILE], 1, tapa::array_partition<tapa::cyclic<2>>, tapa::memcore<tapa::bram>>& buffer_load,
          int n_tiles) {
  for (int tile_id = 0; tile_id < n_tiles; tile_id++) {
    #pragma HLS pipeline off
    auto section = buffer_load.create_section();
    buffer_load.acquire(section);
    auto& buf_ref = section();
    data_type temp;
    for (int j = 0; j < TILE; j+=2) {
      // j is the index of the buffer data (type = data_type)
      #pragma HLS pipeline II=1
      // #pragma HLS unroll factor=2
      buf_ref[j] = a0.read();
      buf_ref[j+1] = a1.read();
    }
  }
}

void Mmap2Stream(tapa::mmap<bits<data_type_mmap>> argmmap,
                 uint64_t n_tiles,
                 tapa::ostream<data_type>& stream0,
                 tapa::ostream<data_type>& stream1) {
  for (int tile_id = 0; tile_id < n_tiles; tile_id++) {
    for (uint64_t i = 0; i < (TILE/PACK_LENGTH); i++) {
      #pragma HLS pipeline II=1
      // i is the index of the mmap stream (type = data_type_mmap)
      auto packvec = tapa::bit_cast<data_type_mmap>(argmmap[tile_id*TILE/(PACK_LENGTH) + i]);
      stream0.write(float(packvec[0]));
      stream1.write(float(packvec[1]));

      // uint64_t temp = mmap[tile_id*TILE + i];
      // stream0 << (uint32_t)(temp & 0xFFFFFFFF); // lower
      // stream1 << (uint32_t)(temp >> 32);        // higher
    }
  }
}


//////////////////
/// STORE
//////////////////
void store(tapa::mmap<data_type_mmap> argmmap,
           tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::cyclic<2>>, tapa::memcore<tapa::bram>>& buffer_store,
           int n_tiles) {
  for (int tile_id = 0; tile_id < n_tiles; tile_id++) {
#pragma HLS pipeline off
    auto section = buffer_store.create_section();
    buffer_store.acquire(section);
    auto& buf_ref = section();
    for (int j = 0; j < TILE/PACK_LENGTH; j++) {
#pragma HLS pipeline II=1
      data_type_mmap packvec;
      packvec[0] = buf_ref[2*j];
      packvec[1] = buf_ref[2*j+1];
      argmmap[tile_id*TILE/(PACK_LENGTH) + j] = packvec;
    }
  }
}

void Stream2Mmap(tapa::istream<data_type>& stream0,
                 tapa::istream<data_type>& stream1,
                 tapa::mmap<bits<data_type_mmap>> argmmap,
                 uint64_t n_tiles) {
  for (int tile_id = 0; tile_id < n_tiles; tile_id++) {
    for (uint64_t blk = 0; blk < NDBLKS; blk++) {
    #pragma HLS pipeline II=1
      // blk is the index of the mmap stream data (type = data_type_mmap)
      data_type_mmap packvec;
      stream0 >> packvec[0];
      stream1 >> packvec[1];
      argmmap[tile_id*TILE/(PACK_LENGTH) + blk] = tapa::bit_cast<bits<data_type_mmap>>(packvec);
    }
  }
}

void storeStream(tapa::ostream<data_type>& c0,
           tapa::ostream<data_type>& c1,
           tapa::ibuffer<data_type[TILE], 1, tapa::array_partition<tapa::cyclic<2>>, tapa::memcore<tapa::bram>>& buffer_c,
           int n_tiles) {
  for (int tile_id = 0; tile_id < n_tiles; tile_id++) {
// #pragma HLS pipeline off
    auto section = buffer_c.create_section();
    buffer_c.acquire(section);
    auto& buf_rf = section();
    for (int j = 0; j < TILE; j+=2) {
#pragma HLS pipeline II=1
      float x[2];
      x[0] = buf_rf[j];
      x[1] = buf_rf[j+1];
      c0.write(x[0]);
      c1.write(x[1]);
    }
  }
}


////////////////
/// WRAPPER
////////////////

void VecAdd(tapa::mmap<bits<data_type_mmap>> vector_a,
            tapa::mmap<bits<data_type_mmap>> vector_b,
            tapa::mmap<bits<data_type_mmap>> vector_c,
            uint64_t n_tiles) {
  tapa::buffer<data_type[TILE], 1, tapa::array_partition<tapa::cyclic<2>>, tapa::memcore<tapa::bram>> buffer_a;
  tapa::buffer<data_type[TILE], 1, tapa::array_partition<tapa::cyclic<2>>, tapa::memcore<tapa::bram>> buffer_b;
  tapa::buffer<data_type[TILE], 1, tapa::array_partition<tapa::cyclic<2>>, tapa::memcore<tapa::bram>> buffer_c;
  tapa::stream<data_type> a_q0("a0");
  tapa::stream<data_type> a_q1("a1");
  tapa::stream<data_type> b_q0("b0");
  tapa::stream<data_type> b_q1("b1");
  tapa::stream<data_type> c_q0("c0");
  tapa::stream<data_type> c_q1("c1");
  tapa::task()
    // .invoke(load, vector_a, buffer_a, n_tiles)
    // .invoke(load, vector_b, buffer_b, n_tiles)
    .invoke(Mmap2Stream, vector_a, n_tiles, a_q0, a_q1)
    .invoke(loadStream, a_q0, a_q1, buffer_a, n_tiles)
    .invoke(Mmap2Stream, vector_b, n_tiles, b_q0, b_q1)
    .invoke(loadStream, b_q0, b_q1, buffer_b, n_tiles)
    .invoke(vadd, buffer_a, buffer_b, buffer_c, n_tiles)
    .invoke(storeStream, c_q0, c_q1, buffer_c, n_tiles)
    .invoke(Stream2Mmap, c_q0, c_q1, vector_c, n_tiles)
    // .invoke(store, vector_c, buffer_c, n_tiles)
    ;
}
