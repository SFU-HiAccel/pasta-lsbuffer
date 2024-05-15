#ifndef __SB_H__
#define __SB_H__
#include <stdint.h>
#include <iostream>
#include "sbif_config.h"

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
  void* brxstreams_p;
  void* btxstreams_p;
public:
  // first abstract the buffercore type
  using buffercore_t  = tapa::buffer<sb_msg_t[npages], concurrency, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
  using brxstreams_t  = tapa::streams<sb_msg_t, iports>;
  using btxstreams_t  = tapa::streams<sb_msg_t, oports>;
  using brxstream_t   = tapa::stream<sb_msg_t>;
  using btxstream_t   = tapa::stream<sb_msg_t>;
  // using drxstreams_t = tapa::streams<sb_msg_t, iports>;
  // using dtxstreams_t = tapa::streams<sb_msg_t, oports>;
  // using crxstreams_t = tapa::streams<addrtype_t, iports>;
  // using ctxstreams_t = tapa::streams<addrtype_t, oports>;

  // constructor
  SharedBuffer() {
    // create temporary pointers to the buffercore and stream-arrays
    buffercore_t* buffercore_p_ = new buffercore_t;
    brxstreams_t* brxstreams_p_ = new brxstreams_t;// rxds("rxdstreams");
    btxstreams_t* btxstreams_p_ = new btxstreams_t;// txds("txdstreams");
    // cast this pointer type to the corresponding private pointers
    buffercore_p = static_cast<void*>(buffercore_p_);
    brxstreams_p = static_cast<void*>(brxstreams_p_);
    btxstreams_p = static_cast<void*>(btxstreams_p_);

    // tapa::streams<datatype_t, iports> rx("rxstream");
    // tapa::streams<datatype_t, oports> tx("txstream");
    validate_config();
  }

  ~SharedBuffer() {
    // delete buffercore pointer upon destruction
    delete static_cast<buffercore_t*>(buffercore_p);
    delete static_cast<brxstreams_t*>(brxstreams_p);
    delete static_cast<btxstreams_t*>(btxstreams_p);
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

  brxstream_t get_iport(sb_portid_t _rx_idx)
  {
    return (static_cast<brxstreams_t*>(brxstreams_p))[_rx_idx];
  }

  btxstream_t get_oport(sb_portid_t _tx_idx)
  {
    return (static_cast<btxstreams_t*>(btxstreams_p))[_tx_idx];
  }

};

#endif // __SB_H__
