#ifndef __SB_H__
#define __SB_H__
#include <stdint.h>
#include <iostream>
#include "sbif_config.h"
#include "tapa.h"
#include "sb_tasks.h"

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

// /**
//  * get_rxq: returns the pointer to a specific index of the request queues array.
//  *          These queues are RX relative to the SB
//  * */
// inline brxq_t sb_get_rxq(sb_portid_t _rx_idx)
// {
//   // use the pointers we declared above (see sb_declarations())
//   return sb_rxqs[_rx_idx];
// }

// /**
//  * get_txq: returns the pointer to a specific index of the response queues array.
//  *          These queues are TX relative to the SB
//  * */
// inline btxq_t sb_get_txq(sb_portid_t _tx_idx)
// {
//   // use the pointers we declared above (see sb_declarations())
//   return sb_txqs[_tx_idx];
// }

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
void sb_task(tapa::istreams<sb_req_t, SB_NXCTRS>& sb_rxqs,
             tapa::ostreams<sb_rsp_t, SB_NXCTRS>& sb_txqs)
{
  // Main interface
  // tapa::streams<sb_req_t, SB_NXCTRS> sb_rxqs("sb_rxqs");
  // tapa::streams<sb_rsp_t, SB_NXCTRS> sb_txqs("sb_txqs");

  // RQR  <--->  RQP
  tapa::stream<sb_std_t> rqr_to_rqp_grab("rqr_to_rqp_grab");
  tapa::stream<sb_std_t> rqr_to_rqp_free("rqr_to_rqp_free");
  tapa::streams<sb_std_t, SB_NXCTRS> rqr_to_rqp_read("rqr_to_rqp_read");
  tapa::streams<sb_std_t, SB_NXCTRS> rqr_to_rqp_write("rqr_to_rqp_write");
  // RQP  <--->  PGM
  tapa::stream<sb_std_t> rqp_to_pgm_grab("rqp_to_pgm_grab");
  tapa::stream<sb_std_t> rqp_to_pgm_free("rqp_to_pgm_free");
  tapa::stream<sb_std_t> pgm_to_rqp_sts("pgm_to_rqp_sts");
  // RQP  <--->  RSG
  tapa::stream<sb_std_t> rqp_to_rsg_grab("rqp_to_rsg_grab");
  tapa::stream<sb_std_t> rqp_to_rsg_free("rqp_to_rsg_free");
  tapa::streams<sb_std_t, SB_NXCTRS> rqp_to_rsg_read("rqp_to_rsg_read");
  tapa::streams<sb_std_t, SB_NXCTRS> rqp_to_rsg_write("rqp_to_rsg_write");

  /// PERFORMANCE CRITICAL STREAMS ///
  // RBUF <--->  RQP
  tapa::streams<sb_std_t, SB_NXCTRS> rbuf_to_rqr("rbuf_to_rqr");
  // RQP  <--->  IHD
  tapa::streams<sb_std_t, SB_NXCTRS> rqp_to_ihd_read("rqp_to_ihd_read");
  // RQP  <--->  OHD
  tapa::streams<sb_std_t, SB_NXCTRS> rqp_to_ohd_write("rqp_to_ohd_write");
  // IHD  <--->  RSG
  tapa::streams<sb_std_t, SB_NXCTRS> ihd_to_rsg_read("ihd_to_rsg_read");
  // IHD  <--->  RSG
  tapa::streams<sb_std_t, SB_NXCTRS> ohd_to_rsg_write("ohd_to_rsg_write");
  // RSG  <--->  RBUF
  tapa::streams<sb_std_t, SB_NXCTRS> rsg_to_rbuf("rsg_to_rbuf");

  // actual buffers
  //buffercore_t backend_pages;

  tapa::task()
    // .invoke<tapa::detach>(rx_arbiter, (*brxqs_p), (*arbit_rxq_p))
    // .invoke<tapa::detach>(tx_arbiter, (*arbit_txq_p), (*btxqs_p))
    .invoke<tapa::detach>(rqr,
                            sb_rxqs,
                            rqr_to_rqp_grab,
                            rqr_to_rqp_free,
                            rqr_to_rqp_read,
                            rqr_to_rqp_write)
    .invoke<tapa::detach>(rqp,
                            pgm_to_rqp_sts,
                            rqr_to_rqp_grab,
                            rqr_to_rqp_free,
                            rqp_to_pgm_grab,
                            rqp_to_pgm_free,
                            rqp_to_rsg_grab,
                            rqp_to_rsg_free,
                            rqr_to_rqp_read,
                            rqr_to_rqp_write,
                            rqp_to_rsg_read,
                            rqp_to_rsg_write,
                            rqp_to_ihd_read,
                            rqp_to_ohd_write)
    .invoke<tapa::detach>(pgm,
                            rqp_to_pgm_grab,
                            rqp_to_pgm_free,
                            pgm_to_rqp_sts)
    .invoke<tapa::detach>(rsg,
                            rqp_to_rsg_grab,
                            rqp_to_rsg_free,
                            rqp_to_rsg_read,
                            rqp_to_rsg_write,
                            ihd_to_rsg_read,
                            ohd_to_rsg_write,
                            sb_txqs) // this will have fe and be rbufs inside
    .invoke<tapa::detach>(ihd,
                            rqp_to_ihd_read,
                            //ihd_to_rsg_read,
                            //backend_pages)
                            ihd_to_rsg_read)
    .invoke<tapa::detach>(ohd,
                            rqp_to_ohd_write,
                            //ohd_to_rsg_write,
                            //backend_pages);
                            ohd_to_rsg_write);
}

#endif // __SB_H__
