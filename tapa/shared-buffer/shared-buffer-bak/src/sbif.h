#ifndef __SBIF_H__
#define __SBIF_H__

#include <stdint.h>
#include <iostream>
#include "tapa.h"
#include "sbif_config.h"
#include "brahma.h"

/**
 * Each Heap interface provides two 'ports':
 * Controlone each for read and write. This is done because the routing for the
 * read and write interfaces might require placement in different regions
 * of the PE's floorplan, especially if the PE is involved in heavy
 * dataflow.
 * Keeping the memory port functionality split like this offers two benefits:
 *  1. Allows the PE to have different 'interface heads'
 *  2. A PE that only does one type of I/O operation defines only one port
 *      and hence saves resources.
*/
template <typename sb_msg_t, uint8_t _rx_depth, uint8_t _tx_depth>
class SBIF {
private:
  uint8_t rx_depth = _rx_depth;
  uint8_t tx_depth = _tx_depth;
  uint8_t rx_head_ptr = 0, rx_tail_ptr = 0;
  uint8_t tx_head_ptr = 0, tx_tail_ptr = 0;
  tapa::istream<sb_msg_t>* rxq_p = nullptr;
  tapa::ostream<sb_msg_t>* txq_p = nullptr;

public:
  sb_msg_t rx_msgs[_rx_depth];
  sb_msg_t tx_msgs[_tx_depth];

  // constructor
  SBIF( tapa::istream<sb_msg_t>& _rxq_p,
        tapa::ostream<sb_msg_t>& _txq_p)
  {
    rxq_p = &_rxq_p;
    txq_p = &_txq_p;
    print_config();
  }

  void do_tx(sb_req_t* req)
  {
    // TODO: should this be req or req.req_msg?
    // Will req.req_msg be updated with the req.fields updates?
    txq_p->write(req->req_msg);
  }

  void do_rx()
  {
    for (bool sbif_rxq_valid;                            \
       !(*rxq_p).eot(sbif_rxq_valid) || !sbif_rxq_valid;) \
    if (sbif_rxq_valid)
    {
      rx_msgs[rx_head_ptr] = rxq_p->read();
    }
  }

  /**
   * Make a grab-request for a page (_npages is assumed = 1 for now)
  */
  sb_pageid_t grab_page(sb_pageid_t _npages)
  {
    sb_req_t req;
    req.c_dn = true;                    // this is a control packet
    req.fields.code = SB_REQ_GRAB_PAGE; // grab page
    req.fields.npages = _npages;        // number of pages
    //begin_atomic();
    do_tx(&req);
    // wait for response from SB. All control requests wait for response.
    do_rx();
    //end_atomic();
    sb_rsp_t rsp;
    rsp.rsp_msg = rx_msgs[rx_head_ptr];
    if(rsp.fields.code == SB_RSP_DONE) {
      return rsp.fields.pageid;
    } else {
      return 0xFFFF;  // magic number impossible page index.
    }
  }

  /**
   * Make a free-request for a page
  */
  bool free_page(sb_pageid_t _pageid)
  {
    sb_req_t req;
    req.c_dn = true;                    // this is a control packet
    req.fields.code = SB_REQ_FREE_PAGE; // free page
    req.fields.pageid = _pageid;        // pageid to free

    //begin_atomic();
    do_tx(&req);
    // wait for response from SB. All control requests wait for response.
    do_rx();
    //end_atomic();
    sb_rsp_t rsp;
    rsp.rsp_msg = rx_msgs[rx_head_ptr];
    if(rsp.fields.code == SB_RSP_DONE) {
      return true;
    } else {
      return false;  // magic number impossible page index.
    }
  }

  sb_msg_t read(sb_pageid_t _pageid, uint16_t _msgid)
  {
    sb_req_t req;
    req.c_dn = true;                    // this is a control packet
    req.fields.code = SB_REQ_READ_MSGS; // read msgs
    req.fields.pageid = _pageid;        // pageid to read
    req.fields.npages = _msgid;         // which msg to read
    // only one msg can be read right now. Add streaming reads later.
    do_tx(&req);
    do_rx();
    // Ideally, there should be a check here to check if it is indeed a data packet
    return rx_msgs[rx_head_ptr];
  }

  bool write(sb_pageid_t _pageid, uint16_t _msgid, sb_msg_t _msg)
  {
    sb_req_t req;
    req.c_dn = true;                    // this is a control packet
    req.fields.code = SB_REQ_WRITE_MSGS;// write msgs
    req.fields.pageid = _pageid;        // pageid to read
    req.fields.npages = _msgid;         // which msg to read
    do_tx(&req);

    req.c_dn = false;                   // this is a data packet
    req.req_msg = _msg;                 // append message
    do_tx(&req);

    do_rx();
    sb_rsp_t rsp = (sb_rsp_t)rx_msgs[rx_head_ptr];
    if(rsp.fields.code == SB_RSP_DONE) {
      return true;
    } else {
      return false;
    }
  }

  /////////////////////////////////////////////////////////////////////////////

  void print_config()
  {
    std::cout << "init heap i/f" << std::endl;
    std::cout << "TX Depth: " << (unsigned)tx_depth << std::endl;
    std::cout << "RX Depth: " << (unsigned)rx_depth << std::endl;
  }

  uint8_t get_tx_depth()
  {
    return tx_depth;
  }

  uint8_t get_rx_depth()
  {
    return rx_depth;
  }

  /////////////////////////////////////////////////////////////////////////////

  void sbif_task()
  {
    return;
  }

};


#endif // __SBIF_H__
