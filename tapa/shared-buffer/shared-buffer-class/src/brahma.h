#ifndef __SB_H__
#define __SB_H__
#include <stdint.h>
#include <iostream>
#include "sbif_config.h"
#include "sbif.h"
#include "tapa.h"

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
  //void* brxqs_p;
  //void* btxqs_p;

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

  uint8_t arbit_rx;
  uint8_t arbit_tx;
  bool tx_available; // must update with notif counter checking
  
public:
  // first abstract the buffercore type
  using buffercore_t  = tapa::buffer<sb_msg_t[npages], concurrency, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
  using brxqs_t  = tapa::streams<sb_msg_t, iports>;
  using btxqs_t  = tapa::streams<sb_msg_t, oports>;
  using brxq_t   = tapa::stream<sb_msg_t>;
  using btxq_t   = tapa::stream<sb_msg_t>;
  brxqs_t* brxqs_p;// = nullptr;
  btxqs_t* btxqs_p;// = nullptr;
  brxq_t* arbit_rxq_p;// = nullptr;
  btxq_t* arbit_txq_p;// = nullptr;
  // using drxqs_t = tapa::streams<sb_msg_t, iports>;
  // using dtxqs_t = tapa::streams<sb_msg_t, oports>;
  // using crxqs_t = tapa::streams<addrtype_t, iports>;
  // using ctxqs_t = tapa::streams<addrtype_t, oports>;

  // constructor
  SharedBuffer() {
    // create temporary pointers to the buffercore and stream-arrays
    buffercore_t* buffercore_p_ = new buffercore_t;
    //brxqs_t* brxqs_p_ = new brxqs_t("sbrxqs");// rxds("rxdstreams");
    //btxqs_t* btxqs_p_ = new btxqs_t("sbtxqs");// txds("txdstreams");

    // cast this pointer type to the corresponding private pointers
    buffercore_p = static_cast<void*>(buffercore_p_);
    //brxqs_p = static_cast<void*>(brxqs_p_);
    //btxqs_p = static_cast<void*>(btxqs_p_);
    brxqs_p = new brxqs_t("sbrxqs");// rxds("rxdstreams");
    btxqs_p = new btxqs_t("sbtxqs");// txds("txdstreams");
    arbit_rxq_p = new brxq_t("sb_arbit_rxq");
    arbit_txq_p = new btxq_t("sb_arbit_txq");

    arbit_tx = 0; arbit_rx = 0; tx_available = false;

    // tapa::streams<datatype_t, iports> rx("rxq");
    // tapa::streams<datatype_t, oports> tx("txq");
    validate_config();
  }

  ~SharedBuffer() {
    // delete (new) pointers upon destruction
    delete static_cast<buffercore_t*>(buffercore_p);
    //delete static_cast<brxqs_t*>(brxqs_p);
    //delete static_cast<btxqs_t*>(btxqs_p);
    delete (brxqs_p);
    delete (btxqs_p);
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

  /**
   * get_rxq: returns the pointer to a specific index of the request queues array.
   *          These queues are RX relative to the SB
   */
  brxq_t get_rxq(sb_portid_t _rx_idx)
  {
    //return (*static_cast<brxqs_t*>(brxqs_p))[_rx_idx];
    return (*brxqs_p)[_rx_idx];
  }

  /**
   * get_txq: returns the pointer to a specific index of the response queues array.
   *          These queues are TX relative to the SB
   */
  btxq_t get_txq(sb_portid_t _tx_idx)
  {
    //return (*static_cast<btxqs_t*>(btxqs_p))[_tx_idx];
    return (*btxqs_p)[_tx_idx];
  }

  // main request handler
  void handle_request(sb_req_t req)
  {
    // if the request is to grab a page
    if(req.fields.code == SB_REQ_GRAB_PAGE)
    {
      // for now, simply set a flag for sending a message
      tx_available = true;
    }
  }

  // arbiters over all RX queues and works on one RX message at once
  void rx_arbiter(tapa::istreams<sb_msg_t, iports>& brxqs,
                  tapa::ostream<sb_msg_t>& arbit_rxq)
  {
    bool rx_available;
    sb_req_t req;
    for(;;)
    {
      rx_available = false;
      req.req_msg = brxqs[arbit_rx].peek(rx_available);
      if(rx_available)
      {
        handle_request(req);
      }
      // round robin
      arbit_rx = (arbit_rx == (iports-1)) ? 0 : arbit_rx;
    }
  }

  // arbiters over all RX queues and works on one TX message at once
  void tx_arbiter(tapa::ostreams<sb_msg_t, oports>& btxqs,
                  tapa::istream<sb_msg_t>& arbit_txq)
  {
    sb_rsp_t rsp;
    for(;;)
    {
      if(tx_available)
      {
        rsp.c_dn = true; 
        rsp.fields.code = SB_RSP_DONE;
        rsp.fields.pageid = 3;
        btxqs[arbit_tx].write(rsp.rsp_msg);
        tx_available = false;
      }
    }
  }

  // task wrapper that should be invoked at the kernel's top wrapper
  void sb_task(tapa::istream<float>& dummy)
  {
    tapa::task()
      .invoke(rx_arbiter, (*brxqs_p), (*arbit_rxq_p))
      .invoke(tx_arbiter, (*btxqs_p), (*arbit_txq_p));
      // .invoke(rx_arbiter, sbo, (*brxqs_p), (*arbit_rxq_p))
      // .invoke(tx_arbiter, sbo, (*btxqs_p), (*arbit_txq_p));
  }

};

#endif // __SB_H__
