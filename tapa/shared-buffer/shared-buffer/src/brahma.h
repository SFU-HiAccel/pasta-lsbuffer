#ifndef __SB_H__
#define __SB_H__
#include <stdint.h>
#include <iostream>
#include "sbif_config.h"
#include "sbif.h"

/**
 * datatype_t --> type of the data packet (page) that will be ID-ed.
 *                All headers and bookkeeping is appended within sharedBuffer.
 * iports     --> Number of input ports (producers)
 * oports     --> Number of output ports (consumers)
 * npages     --> Total number of buffer-blocks (pages) to maintain
 * concurrency--> Number of concurrent operations to allow.
 *                concurrency <= (iports+oports)
*/
template <typename sb_msg_t, sb_pageid_t npages, sb_portid_t iports, sb_portid_t oports, uint8_t concurrency>
class SharedBuffer {
private:
  using sb_t = SharedBuffer<sb_msg_t, SB_NUM_PAGES, SB_NRX, SB_NTX, (SB_NRX+SB_NTX)>;
  friend sb_t;
  // declare buffer pointer as a void type,
  // dimension information is stored in the public variable `buffercore_t`
  void* buffercore_p;
  void* brxqs_p;
  void* btxqs_p;

  typedef struct
  {
    union{
      uint8_t valid : 1;
      uint8_t pad   : 7;
    }header;
    sb_pageid_t pageid;
  }sb_metadata_t;

  sb_metadata_t metadata_t[SB_NUM_PAGES] = {0};
  bool valid_pages[8][(SB_NUM_PAGES>>3)] = {0};

  // used to set pageids in metadata when that page is requested for the first time
  sb_pageid_t pageid_init_counter = 0;
  
public:
  // first abstract the buffercore type
  using buffercore_t  = tapa::buffer<sb_msg_t[npages], concurrency, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
  using brxqs_t  = tapa::streams<sb_msg_t, iports>;
  using btxqs_t  = tapa::streams<sb_msg_t, oports>;
  using brxq_t   = tapa::stream<sb_msg_t>;
  using btxq_t   = tapa::stream<sb_msg_t>;
  // using drxqs_t = tapa::streams<sb_msg_t, iports>;
  // using dtxqs_t = tapa::streams<sb_msg_t, oports>;
  // using crxqs_t = tapa::streams<addrtype_t, iports>;
  // using ctxqs_t = tapa::streams<addrtype_t, oports>;

  // constructor
  SharedBuffer() {
    // create temporary pointers to the buffercore and stream-arrays
    buffercore_t* buffercore_p_ = new buffercore_t;
    brxqs_t* brxqs_p_ = new brxqs_t("sbrxqs");// rxds("rxdstreams");
    btxqs_t* btxqs_p_ = new btxqs_t("sbtxqs");// txds("txdstreams");

    // cast this pointer type to the corresponding private pointers
    buffercore_p = static_cast<void*>(buffercore_p_);
    brxqs_p = static_cast<void*>(brxqs_p_);
    btxqs_p = static_cast<void*>(btxqs_p_);

    // tapa::streams<datatype_t, iports> rx("rxq");
    // tapa::streams<datatype_t, oports> tx("txq");
    validate_config();
  }

  ~SharedBuffer() {
    // delete (new) pointers upon destruction
    delete static_cast<buffercore_t*>(buffercore_p);
    delete static_cast<brxqs_t*>(brxqs_p);
    delete static_cast<btxqs_t*>(btxqs_p);
  }

  /**
   * validate the configuration of the buffer. Expected to be a bunch of
   * static assertions with some helpful messages.
   * This function breaks compilation on an error.
   * */ 
  bool validate_config()
  {
    // print some info first
    std::cout << "shared buff   " << std::endl;
    std::cout << "i/p ports   : " << (unsigned)iports << std::endl;
    std::cout << "o/p ports   : " << (unsigned)oports << std::endl;
    std::cout << "concurrency : " << concurrency << std::endl;
    std::cout << "typesize    : " << sizeof(sb_msg_t) << std::endl;
    std::cout << "totalsize   : " << sizeof(buffercore_t) << std::endl;

    static_assert(concurrency <= (iports + oports),\
    "Cannot provide concurrency more than total i/o-ports");
  }

  brxq_t get_rxq(sb_portid_t _rx_idx)
  {
    return (*static_cast<brxqs_t*>(brxqs_p))[_rx_idx];
  }

  btxq_t get_txq(sb_portid_t _tx_idx)
  {
    return (*static_cast<btxqs_t*>(btxqs_p))[_tx_idx];
  }

};

#endif // __SB_H__
