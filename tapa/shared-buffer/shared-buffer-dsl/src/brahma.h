#ifndef __SB_H__
#define __SB_H__
#include <stdint.h>
#include <iostream>
#include "sbif_config.h"
#include "sbif.h"
#include "tapa.h"
#include "sb_tasks.h"

/**
 * datatype_t --> type of the data packet (page) that will be ID-ed.
 *                All headers and bookkeeping is appended within sharedBuffer.
 * SB_NRX     --> Number of input ports (producers)
 * SB_NTX     --> Number of output ports (consumers)
 * npages     --> Total number of buffer-blocks (pages) to maintain
 * concurrency--> Number of concurrent operations to allow.
 *                concurrency <= (SB_NRX+SB_NTX)
*/
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

// create temporary pointers to the buffercore and stream-arrays
// buffercore_t* buffercore_p_ = new buffercore_t;
// cast this pointer type to the corresponding private pointers
// buffercore_p = static_cast<void*>(buffercore_p_);

// declare all stream types
using buffercore_t  = tapa::buffer<sb_msg_t[npages], 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
using brxqs_t  = tapa::streams<sb_msg_t, SB_NRX>;
using btxqs_t  = tapa::streams<sb_msg_t, SB_NTX>;
using brxq_t   = tapa::stream<sb_msg_t>;
using btxq_t   = tapa::stream<sb_msg_t>;
brxqs_t* brxqs_p;// = nullptr;
btxqs_t* btxqs_p;// = nullptr;
brxq_t* arbit_rxq_p;// = nullptr;
btxq_t* arbit_txq_p;// = nullptr;


/**
 * a list of lines that must be patched directly
 * before calling tapa::task().invokes
 * */
inline void sb_declarations()
{
  brxqs_p = new brxqs_t("sbrxqs");
  btxqs_p = new btxqs_t("sbtxqs");
  arbit_rxq_p = new brxq_t("sb_arbit_rxq");
  arbit_txq_p = new btxq_t("sb_arbit_txq");
  arbit_tx = 0; arbit_rx = 0; tx_available = false;
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
  std::cout << "i/p ports   : " << (unsigned)SB_NRX << std::endl;
  std::cout << "o/p ports   : " << (unsigned)SB_NTX << std::endl;
  std::cout << "concurrency : " << (SB_NRX+SB_NTX) << std::endl;
  std::cout << "typesize    : " << sizeof(sb_msg_t) << std::endl;
  std::cout << "totalsize   : " << sizeof(buffercore_t) << std::endl;

  static_assert(SB_NRX+SB_NTX <= (SB_NRX+SB_NTX),\
  "Cannot provide concurrency more than total i/o-ports");
  return true;
}

/**
 * get_rxq: returns the pointer to a specific index of the request queues array.
 *          These queues are RX relative to the SB
 * */
brxq_t sb_get_rxq(sb_portid_t _rx_idx)
{
  // use the pointers we declared above (see sb_declarations())
  return (*brxqs_p)[_rx_idx];
}

/**
 * get_txq: returns the pointer to a specific index of the response queues array.
 *          These queues are TX relative to the SB
 * */
btxq_t sb_get_txq(sb_portid_t _tx_idx)
{
  // use the pointers we declared above (see sb_declarations())
  return (*btxqs_p)[_tx_idx];
}

///////////////////////////////////
///     SHARED BUFFER TASKS     ///
///////////////////////////////////


// main request handler
void request_handler(tapa::ostream<sb_msg_t>& arbit_rxq)
{
  sb_req_t req;
  // if the request is to grab a page
  if(req.fields.code == SB_REQ_GRAB_PAGE)
  {
    // for now, simply set a flag for sending a message
    tx_available = true;
  }
}

// arbiters over all RX queues and works on one RX message at once
void rx_arbiter(tapa::istreams<sb_msg_t, SB_NRX>& brxqs,
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
      arbit_rxq.write(req.req_msg);
    }
    // round robin
    arbit_rx = (arbit_rx == (SB_NRX-1)) ? 0 : arbit_rx;
  }
}

// arbiters over all RX queues and works on one TX message at once
void tx_arbiter(tapa::ostreams<sb_msg_t, SB_NTX>& btxqs,
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
    .invoke<tapa::detach>(rx_arbiter, (*brxqs_p), (*arbit_rxq_p))
    .invoke<tapa::detach>(tx_arbiter, (*arbit_txq_p), (*btxqs_p))
    .invoke<tapa::detach>(fe_request_handler, (*arbit_rxq_p), (*req_rbuf))
    .invoke<tapa::detach>(fe_response_handler, rsp_rbuf, (*arbit_txq_p))
    .invoke<tapa::detach>(datapath, ) // this will have fe and be rbufs inside
    .invoke<tapa::detach>(be_request_handler, (*req_rbuf), )
    .invoke<tapa::detach>(be_response_handler, (*btxqs_p), (*arbit_txq_p))
}

#endif // __SB_H__
