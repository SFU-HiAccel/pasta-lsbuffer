#include <tapa.h>
#include "add.h"
#include "brahma.h"

using sb_t = SharedBuffer<sb_msg_t, SB_NUM_PAGES, SB_NRX, SB_NTX, (SB_NRX+SB_NTX)>;

void Mmap2Stream(tapa::mmap<const float> mmap,
                 tapa::ostream<float>& stream) {
  for (uint64_t i = 0; i < N; ++i) {
    stream << mmap[i];
  }
}

void Stream2Mmap(tapa::istream<float>& stream,
                  tapa::mmap<float> mmap) {
  for (uint64_t i = 0; i < N; ++i) {
    stream >> mmap[i];
  }
}

void vadd(tapa::istream<float>& a,
          tapa::istream<float>& b,
          tapa::ostream<float>& c,
          uint64_t n) {
  for (uint64_t i = 0; i < N; ++i) {
    c << (a.read() + b.read());
  }
}

void task1( tapa::istream<float>& vector_a,
            tapa::ostream<sb_msg_t>& tx_task1_to_sb,
            tapa::istream<sb_msg_t>& rx_sb_to_task1,
            tapa::ostream<float>& tx_task1_to_task2)
{
  SBIF<sb_msg_t, 2, 2> sbif1(rx_sb_to_task1, tx_task1_to_sb);
  sb_msg_t rsp;
  rsp = sbif1.grab_page(1);
  for (uint64_t i = 0; i < N; ++i)
  {
    tx_task1_to_task2 << vector_a.read();
  }
}

void task2( tapa::istream<float>& vector_b,
            tapa::ostream<float>& vector_c,
            tapa::ostream<sb_msg_t>& tx_task2_to_sb,
            tapa::istream<sb_msg_t>& rx_sb_to_task2,
            tapa::istream<float>& rx_task1_to_task2)
{
  for (uint64_t i = 0; i < N; ++i)
  {
    // vector_c << (float)(0);
    vector_c << vector_b.read() + rx_task1_to_task2.read();
    // printf("%f %f\n", vector_b.read() + rx_task1_to_task2.read());
  }
}

////////////////
/// WRAPPER
////////////////

void VecAdd(tapa::mmap<const float> vector_a,
            tapa::mmap<const float> vector_b,
            tapa::mmap<float> vector_c) {
  tapa::stream<float> a_q("a");
  tapa::stream<float> b_q("b");
  tapa::stream<float> c_q("c");
  tapa::stream<float> task1_to_task2_pageinfo ("task1_to_task2_pageinfo");
  tapa::stream<float> dummy ("dummy");
  std::cout << "===" << std::endl;
  sb_declarations();
  
  tapa::task()
    .invoke(Mmap2Stream, vector_a, a_q)
    .invoke(Mmap2Stream, vector_b, b_q)
    .invoke(sb_task, dummy)
    // .invoke(vadd, a_q, b_q, c_q, n_tiles)
    .invoke(task1, a_q,      sb_get_rxq(0), sb_get_txq(0), task1_to_task2_pageinfo)
    .invoke(task2, b_q, c_q, sb_get_rxq(1), sb_get_txq(1), task1_to_task2_pageinfo)
    .invoke(Stream2Mmap, c_q, vector_c);
}
