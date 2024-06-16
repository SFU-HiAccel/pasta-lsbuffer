#include <tapa.h>
#include "add.h"
#include "brahma.h"

#ifndef __SYNTHESIS__
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

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
          tapa::ostream<float>& c) {
  for (uint64_t i = 0; i < N; ++i) {
    c << (a.read() + b.read());
  }
}

void task1( tapa::istream<float>& vector_a,
            tapa::ostream<sb_req_t>& tx_task1_to_sb,
            tapa::istream<sb_rsp_t>& rx_sb_to_task1,
            tapa::ostream<float>& tx_task1_to_task2)
{

  // GRAB REQUEST
  sb_req_t request = {0};
  request.c_dn = 1;
  request.fields.code = SB_REQ_GRAB_PAGE;
  request.fields.pageid = 1;  // number of pages to allocate
  tx_task1_to_sb << request;

  // now wait for the response to be received
  sb_rsp_t rsp;
  rsp = rx_sb_to_task1.read();
  DEBUG_PRINT("[TASK1][G]: Grab: Rsp: %lu\n", (uint64_t)rsp.fields.pageid);
  sb_pageid_t task1_page = rsp.fields.pageid;

  // WRITE_REQUEST
  request = {0};
  request.c_dn = 1;
  request.fields.code = SB_REQ_WRITE_MSGS;
  request.fields.length = 4;
  request.fields.pageid = task1_page;
  DEBUG_PRINT("[TASK1][W]: Sending request: %lx\n", (uint64_t)request.req_msg);
  tx_task1_to_sb << request;
  // Data
  request.c_dn = 0;
  request.req_msg = 0xDEADBEEF;
  for(int i = 0; i < 4; i++)
  {
    tx_task1_to_sb << request;
    request.req_msg += 1;
    //tx_task1_to_sb << request;
    //request.req_msg += 1;
    //tx_task1_to_sb << request;
    //request.req_msg += 1;
    //tx_task1_to_sb << request;
  }

  // now wait for the response
  rsp = rx_sb_to_task1.read();
  DEBUG_PRINT("[TASK1][W]: %lu %lu\n", (uint64_t)rsp.fields.code, (uint64_t)rsp.fields.pageid);
  
  for (uint64_t i = 0; i < N; ++i)
  {
    tx_task1_to_task2 << vector_a.read();
  }
}

void task2( tapa::istream<float>& vector_b,
            tapa::ostream<float>& vector_c,
            tapa::ostream<sb_req_t>& tx_task2_to_sb,
            tapa::istream<sb_rsp_t>& rx_sb_to_task2,
            tapa::istream<float>& rx_task1_to_task2)
{
  for (uint64_t i = 0; i < N; ++i)
  {
    // vector_c << (float)(0);
    vector_c << vector_b.read() + rx_task1_to_task2.read();
    // printf("%f %f\n", vector_b.read() + rx_task1_to_task2.read());
  }
  DEBUG_PRINT("[TASK2]: Pushed Vector C\n");
  // READ REQUEST
  sb_req_t request1 = {0};
  request1.c_dn = 1;
  request1.fields.code = SB_REQ_READ_MSGS;
  request1.fields.length = 4;
  request1.fields.pageid = 0;
  tx_task2_to_sb << request1;
  // Data
  sb_rsp_t rsp;
  // now wait for the response
  for(uint8_t i = 0; i < 4; i++)
  {
    rsp = rx_sb_to_task2.read();
    DEBUG_PRINT("[TASK2][R]: Msg: %lx\n", (uint64_t)rsp.rsp_msg);
  }
  DEBUG_PRINT("[TASK2]: Done\n");
}

///////////////////////////////////////////////////////////////////////////////
///                SHARED BUFFER SUBTASKS AND GORY DETAILS                  ///
///////////////////////////////////////////////////////////////////////////////


//void loopback(tapa::istreams<sb_req_t, SB_NXCTRS>& brxqs,
//              tapa::ostreams<sb_rsp_t, SB_NXCTRS>& btxqs)
//{
//    for(bool valid[SB_NXCTRS];;)
//    {
//        for(uint8_t xctr = 0; xctr < SB_NXCTRS; xctr++) // this check is being done for each xctr stream being rxed
//        {
//        #pragma HLS unroll    // full unroll by a factor of SB_NXCTRS
//            sb_req_t req = brxqs[xctr].peek(valid[xctr]);
//            sb_rsp_t rsp = {0};
//            if(valid[xctr])
//            {
//                rsp.c_dn = 1;
//                rsp.fields.code = req.fields.code;
//                rsp.fields.length = req.fields.length;
//                btxqs[xctr] << rsp;
//                req = brxqs[xctr].read();
//            }
//        }
//    }
//}


// Request Router
void rqr(tapa::istreams<sb_req_t, SB_NXCTRS>& brxqs,
        tapa::ostream<sb_std_t>& rqr_to_rqp_grab,
        tapa::ostream<sb_std_t>& rqr_to_rqp_free,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& rqr_to_rqp_read,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& rqr_to_rqp_write) {

  bool valid[SB_NXCTRS];
  bool fwd_rqp_free[SB_NXCTRS];
  bool fwd_rqp_grab[SB_NXCTRS];
  bool fwd_rqp_read[SB_NXCTRS];
  bool fwd_rqp_write[SB_NXCTRS];
  sb_req_t req[SB_NXCTRS];
  sb_std_t std_req[SB_NXCTRS];
  for(;;)
  {
    for(uint8_t xctr = 0; xctr < SB_NXCTRS; xctr++) // this check is being done for each xctr stream being rxed
    {
    #pragma HLS unroll    // full unroll by a factor of SB_NXCTRS
      // peek whether value is available
      req[xctr] = brxqs[xctr].peek(valid[xctr]);   // TODO: is this OK? Or does `req` need to be an array for the loop to be unrolled?
      fwd_rqp_free[xctr]  = valid[xctr] && (req[xctr].fields.code == SB_REQ_FREE_PAGE);
      fwd_rqp_grab[xctr]  = valid[xctr] && (req[xctr].fields.code == SB_REQ_GRAB_PAGE);
      fwd_rqp_read[xctr]  = valid[xctr] && (req[xctr].fields.code == SB_REQ_READ_MSGS);
      fwd_rqp_write[xctr] = valid[xctr] && (req[xctr].fields.code == SB_REQ_WRITE_MSGS);

      std_req[xctr] = req_to_std(req[xctr]);
      if(fwd_rqp_free[xctr])        // force write into the free queue
      {
        std_req[xctr].fields.index = (sb_pageid_t)xctr;   // write xctr index into the index field
        rqr_to_rqp_free         << std_req[xctr];
        brxqs[xctr].read();
        DEBUG_PRINT("[RQR][xctr:%2d][F]: RX request\n", xctr);
      }
      else if(fwd_rqp_grab[xctr])   // force write into the grab queue
      {
        std_req[xctr].fields.index = (sb_pageid_t)xctr;   // write xctr index into the index field
        rqr_to_rqp_grab         << std_req[xctr];
        brxqs[xctr].read();
        DEBUG_PRINT("[RQR][xctr:%2d][G]: RX request\n", xctr);
      }
      else if(fwd_rqp_read[xctr])   // force write into the read queue
      {
        rqr_to_rqp_read[xctr]   << std_req[xctr];
        brxqs[xctr].read();
        DEBUG_PRINT("[RQR][xctr:%2d][R]: RX request\n", xctr);
      }
      else if(fwd_rqp_write[xctr])  // force write into the write queue
      {
        rqr_to_rqp_write[xctr]  << std_req[xctr];
        sb_pageid_t nmsgs = req[xctr].fields.length;
        brxqs[xctr].read();
        DEBUG_PRINT("[RQR][xctr:%2d][W]: RX request\n", xctr);
        while(nmsgs--)
        {
          // forward all write-messages to the respective queue
          sb_req_t wreq;
          DEBUG_PRINT("[RQR][xctr:%2d][W]: waiting for %d message(s)\n", xctr, int(nmsgs+1));
          wreq = brxqs[xctr].read();
          rqr_to_rqp_write[xctr] << req_to_std(wreq);
        }
      }
      else
      {
        if(valid[xctr])
        {
          DEBUG_PRINT("[RQR][xctr:%2d][X]: Received unknown request\n", xctr);
          DEBUG_PRINT("[RQR][xctr:%2d][X]: req.c_d          = %d\n", xctr, (int)req[xctr].c_dn);
          DEBUG_PRINT("[RQR][xctr:%2d][X]: req.fields.code  = %x\n", xctr, req[xctr].fields.code);
          DEBUG_PRINT("[RQR][xctr:%2d][X]: req.req_msg      = %lx\n", xctr, req[xctr].req_msg);
          DEBUG_PRINT("%d %d\n", valid[xctr], fwd_rqp_write[xctr]);
          assert(false);  // must never encounter this scenario
        }
      }
    }
  }
}


/**
 * Task     : Request Parser
 * Purpose  : The Request Parser is the intercept between blocking and
 *              non-blocking requests. Non-blocking requests are directly
 *              forwarded to the I/OHD while the GRAB and FREE queues
 *              are intercepted and the performance optimised IOHD streams
 *              are gated while this happens.
 * TODO: Currently the IOHD streams are only 1 each for read and write. It
 *          needs to be made into a vector of streams based on the concurrency
 * */
void rqp(tapa::istream<sb_std_t>& pgm_to_rqp_sts,
        tapa::istream<sb_std_t>& rqr_to_rqp_grab,
        tapa::istream<sb_std_t>& rqr_to_rqp_free,
        tapa::ostream<sb_std_t>& rqp_to_pgm_grab,
        tapa::ostream<sb_std_t>& rqp_to_pgm_free,
        tapa::ostream<sb_std_t>& rqp_to_rsg_grab,
        tapa::ostream<sb_std_t>& rqp_to_rsg_free,
        tapa::istreams<sb_std_t, SB_NXCTRS>& rqr_to_rqp_read,
        tapa::istreams<sb_std_t, SB_NXCTRS>& rqr_to_rqp_write,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& rqp_to_rsg_read,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& rqp_to_rsg_write,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& rqp_to_ihd_read,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& rqp_to_ohd_write) {

  sb_std_t g_fwd_req, f_fwd_req, nb_fwd_req[SB_NXCTRS], pgm_rsp;
  // Free > Grab > Read > Write
  for(bool vld_rqr_g, vld_rqr_f, vld_pgm_sts, vld_rqr_w[SB_NXCTRS], vld_rqr_r[SB_NXCTRS];;)
  {
    // blocking forwards
    f_fwd_req = rqr_to_rqp_free.peek(vld_rqr_f);
    g_fwd_req = rqr_to_rqp_grab.peek(vld_rqr_g);
    pgm_rsp = pgm_to_rqp_sts.peek(vld_pgm_sts);
    if(vld_pgm_sts)
    {
      // read code and compare the lower 4 bits to check request type
      if((pgm_rsp.fields.code & 0xF) == SB_REQ_FREE_PAGE)
      {
        DEBUG_PRINT("[RQP][xctr:%2d][F]: fwd rsp --> RSG\n", pgm_rsp.fields.index);
        rqp_to_rsg_free << pgm_to_rqp_sts.read();
      }
      else if((pgm_rsp.fields.code & 0xF) == SB_REQ_GRAB_PAGE)
      {
        DEBUG_PRINT("[RQP][xctr:%2d][G]: fwd rsp --> RSG\n", pgm_rsp.fields.index);
        rqp_to_rsg_grab << pgm_to_rqp_sts.read();
      }
    }
    else if(vld_rqr_f)   // free queue has some message
    {
      DEBUG_PRINT("[RQP][xctr:%2d][F]: fwd req --> PGM\n", f_fwd_req.fields.index);
      rqp_to_pgm_free << rqr_to_rqp_free.read();
      // hard wait on pgm:status queue
      pgm_rsp = pgm_to_rqp_sts.peek(vld_pgm_sts);
      // rqp_to_rsg_free << pgm_to_rqp_sts.read();    // hard wait on pgm:status queue
      DEBUG_PRINT("[RQP][xctr:%2d][F]: rcv rsp <-- PGM\n", f_fwd_req.fields.index);
    }
    else if(vld_rqr_g)   // grab queue has some message
    {
      DEBUG_PRINT("[RQP][xctr:%2d][G]: fwd req --> PGM\n", g_fwd_req.fields.index);
      rqp_to_pgm_grab << rqr_to_rqp_grab.read();
      // hard wait on pgm:status queue
      pgm_rsp = pgm_to_rqp_sts.peek(vld_pgm_sts);
      // rqp_to_rsg_free << pgm_to_rqp_sts.read();    // hard wait on pgm:status queue
      DEBUG_PRINT("[RQP][xctr:%2d][G]: rcv rsp <-- PGM\n", g_fwd_req.fields.index);
    }
    else {}

    // WRITE: expecting this to be unrolled and done in parallel with free/grab
    for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
    {
    #pragma HLS unroll
      nb_fwd_req[xctr] = rqr_to_rqp_write[xctr].peek(vld_rqr_w[xctr]);
      if(vld_rqr_w[xctr])
      {
        rqp_to_rsg_write[xctr] << nb_fwd_req[xctr]; // ctrl pkt for RSG to track
        // consume ctrl:write message to consume data further
        nb_fwd_req[xctr] = rqr_to_rqp_write[xctr].read();
        // get the number of messages in this burst
        sb_pageid_t msgs = nb_fwd_req[xctr].fields.length;  // TODO: doesn't look like `msgs` or `nb_fwd_req`  need to be created as an array because it is internal to the loop
        DEBUG_PRINT("[RQP][xctr:%2d][W]: req.nmsgs:%2d\n", xctr, msgs);
        rqp_to_ohd_write[xctr] << nb_fwd_req[xctr];
        sb_std_t data;
        while(msgs)
        {
          DEBUG_PRINT("[RQP][xctr:%2d][W]: waiting for %d more message(s)\n", xctr, msgs);
          data = rqr_to_rqp_write[xctr].read();  // block for write data
          // data.c_dn = 0;  // must already be a data packet
          rqp_to_ohd_write[xctr] << data;   // data pkts for OHD to track
          msgs--;
          DEBUG_PRINT("[RQP][xctr:%2d][W]: fwd req --> OHD\n", xctr);
        }
        DEBUG_PRINT("[RQP][xctr:%2d][W]: fwd req --> RSG\n", xctr);
      }
    }

    // READ: expecting this to be unrolled and done in parallel with free/grab
    for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
    {
    #pragma HLS unroll
      // non-blocking forwards
      nb_fwd_req[xctr] = rqr_to_rqp_read[xctr].peek(vld_rqr_r[xctr]);
      if(vld_rqr_r[xctr])
      {
        sb_pageid_t msgs = nb_fwd_req[xctr].fields.length;  // TODO: doesn't look like `msgs` or `nb_fwd_req`  need to be created as an array because it is internal to the loop
        DEBUG_PRINT("[RQP][xctr:%2d][R]: req.nmsgs:%2d\n", xctr, msgs);
        rqp_to_ihd_read[xctr] << nb_fwd_req[xctr];  // for data
        DEBUG_PRINT("[RQP][xctr:%2d][R]: fwd req --> IHD\n", xctr);
        rqp_to_rsg_read[xctr] << nb_fwd_req[xctr];  // for RSG to track
        DEBUG_PRINT("[RQP][xctr:%2d][R]: fwd req --> RSG\n", xctr);
        nb_fwd_req[xctr] = rqr_to_rqp_read[xctr].read();
      }
    }
  }
}


// Function to find the index of the first 0 bit in a number
inline uint8_t find_first_zero_bit_index(uint8_t num) {
  for (uint8_t i = 0; i < 8; i++) {
    if (!(num & (1 << i))) {
      return i;
    }
  }
  return 0xFF;  // error code
}

/**
 * Task     : Page Manager
 * Purpose  : The page-manager is responsible for maintaining all information
 *              related to the page allocation/deallocations
 *              THIS PATH IS NOT OPTIMISED FOR PERFORMANCE.
 *
*/
void pgm(tapa::istream<sb_std_t>& rqp_to_pgm_grab,
        tapa::istream<sb_std_t>& rqp_to_pgm_free,
        tapa::ostream<sb_std_t>& pgm_to_rqp_sts) {

  // sb_metadata_t metadata[SB_NUM_PAGES] = {0};
  uint8_t valid[SB_NUM_PAGES>>3] = {0};

  // sb_metadata_t free_md, grab_md;
  sb_pageid_t free_vld8_pre, free_vld8_new, free_pageid_in8;
  sb_pageid_t grab_vld8_pre, grab_vld8_new, grab_pageid_in8, grab_avl_idx;
  bool grab_avl;
  sb_std_t fwd_rsp_f, fwd_rsp_g, rsp;
  sb_pageid_t grab_pageid;
  sb_pageid_t free_pageid;

  // initialise lookup array
  uint8_t avl_page_lut[255] = {0};
  for (uint16_t i = 0; i < 255; i++)
  {
    avl_page_lut[i] = find_first_zero_bit_index(i);
  }

  for(bool vld_g, vld_f;;)
  {
    // clear existing response
    rsp.c_dn = 1;
    rsp.std_msg = 0;
    fwd_rsp_f = rqp_to_pgm_free.peek(vld_f);
    fwd_rsp_g = rqp_to_pgm_grab.peek(vld_g);
    if(vld_f)       // handle page deallocation
    {
      DEBUG_PRINT("[PGM][xctr:%2d][F]: pageid %d\n", fwd_rsp_f.fields.index, fwd_rsp_f.fields.pageid);
      
      // update page info
      free_pageid     = fwd_rsp_f.fields.pageid;                      // get the pageid
      free_vld8_pre   = valid[free_pageid>>3];                      // get the 8-bit valid byte
      free_pageid_in8 = free_pageid & 0x7;                          // get the 3LSBs from `pageid`
      free_vld8_new   = free_vld8_pre & ~(1 << free_pageid_in8);    // unset this specific bit
      valid[free_pageid>>3] = free_vld8_new;

      // send response
      rsp.fields.code = SB_RSP_DONE | SB_REQ_FREE_PAGE;
      pgm_to_rqp_sts << rsp;

      rqp_to_pgm_free.read(); // consume the token

      DEBUG_PRINT("[PGM][xctr:%2d][F]: fwd rsp --> RQP\n", fwd_rsp_f.fields.index);
    }

    else if(vld_g)  // handle page allocation
    {
      DEBUG_PRINT("[PGM][xctr:%2d][G]: %d page(s)\n", fwd_rsp_g.fields.index, fwd_rsp_g.fields.pageid);

      grab_avl = false;
      // loop around all bytes and find the available index
      PGM_G_BIN_SEARCH: for(sb_pageid_t i = 0; i < (SB_NUM_PAGES>>3); i++) {
        if(valid[i] != 0xFF) {
          grab_avl_idx = i;
          grab_avl = true;
          break;
        }
      }
      if(grab_avl)
      {
        // update page info
        grab_vld8_pre   = valid[grab_avl_idx];                        // get the 8-bit valid byte
        grab_pageid_in8 = avl_page_lut[grab_vld8_pre];                // find a page which is keeping the bin empty
        grab_vld8_new   = grab_vld8_pre & ~(1 << grab_pageid_in8);    // unset this specific bit
        valid[free_pageid>>3] = free_vld8_new;

        grab_pageid     = (grab_avl_idx << 3) | grab_pageid_in8;      // form the pageid
        rsp.fields.code = SB_RSP_DONE | SB_REQ_GRAB_PAGE;
        rsp.fields.pageid = grab_pageid;
      }
      else
      {
        // generate the response
        rsp.fields.code = SB_RSP_FAIL | SB_REQ_GRAB_PAGE;
        // hardcoded to return pageid 2
        rsp.fields.pageid = 0xFFFF;
      }

      // consume the token
      pgm_to_rqp_sts << rsp;
      rqp_to_pgm_grab.read();

      DEBUG_PRINT("[PGM][xctr:%2d][G]: fwd rsp --> RQP\n", fwd_rsp_g.fields.index);
    }
    else {}
  }
}

void rsg(tapa::istream<sb_std_t>& rqp_to_rsg_grab,
        tapa::istream<sb_std_t>& rqp_to_rsg_free,
        tapa::istreams<sb_std_t, SB_NXCTRS>& rqp_to_rsg_read,
        tapa::istreams<sb_std_t, SB_NXCTRS>& rqp_to_rsg_write,
        tapa::istreams<sb_std_t, SB_NXCTRS>& ihd_to_rsg_read,
        tapa::istreams<sb_std_t, SB_NXCTRS>& ohd_to_rsg_write,
        tapa::ostreams<sb_rsp_t, SB_NXCTRS>& btxqs) {

  // Free > Grab > Read > Write
  sb_std_t nb_fwd_rsp, b_fwd_rsp[SB_NXCTRS];
  sb_rsp_t fwd_rsp[SB_NXCTRS];
  for(bool vld_rqr_g, vld_rqr_f, vld_rqr_r[SB_NXCTRS], vld_rqr_w[SB_NXCTRS];;)
  {
    // non-blocking forwards
    nb_fwd_rsp = rqp_to_rsg_free.peek(vld_rqr_f);
    if(vld_rqr_f)   // free queue has some message
    {
      sb_portid_t xctr = (sb_portid_t)nb_fwd_rsp.fields.length;
      DEBUG_PRINT("[RSG][xctr:%2d][F]: TX response\n", xctr);
      btxqs[xctr] << std_to_rsp(nb_fwd_rsp);
      nb_fwd_rsp = rqp_to_rsg_free.read();
    }
    nb_fwd_rsp = rqp_to_rsg_grab.peek(vld_rqr_g);
    if(vld_rqr_g)   // grab queue has some message
    {
      sb_portid_t xctr = (sb_portid_t)nb_fwd_rsp.fields.length;
      DEBUG_PRINT("[RSG][xctr:%2d][G]: TX response\n", xctr);
      btxqs[xctr] << std_to_rsp(nb_fwd_rsp);
      nb_fwd_rsp = rqp_to_rsg_grab.read();
    }

    // READS
    for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
    {
    #pragma HLS unroll
      // blocking forwards
      b_fwd_rsp[xctr] = rqp_to_rsg_read[xctr].peek(vld_rqr_r[xctr]);
      if(vld_rqr_r[xctr])
      {
        // get the number of messages in this burst
        sb_pageid_t msgs = b_fwd_rsp[xctr].fields.length;
        rqp_to_rsg_read[xctr].read();             // consume read ctrl pkt from RQP
        sb_std_t data;
        while(msgs--)
        {
          DEBUG_PRINT("[RSG][xctr:%2d][R]: waiting for %d message(s)\n", xctr, msgs+1);
          data = ihd_to_rsg_read[xctr].read();  // block for read data pkts from IHD
          // data.c_dn = 0;  // must already be a data packet
          btxqs[xctr] << std_to_rsp(data);
        }
        DEBUG_PRINT("[RSG][xctr:%2d][R]: TX response\n", xctr);
        // consume the value from the ctrl:read queue
        b_fwd_rsp[xctr] = rqp_to_rsg_read[xctr].read();
      }
    }

    // WRITES
    for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
    {
    #pragma HLS unroll
      b_fwd_rsp[xctr] = rqp_to_rsg_write[xctr].peek(vld_rqr_w[xctr]);
      if(vld_rqr_w[xctr])
      {
        // need to wait for write response from OHD
        sb_std_t data;
        data = ohd_to_rsg_write[xctr].read();  // block for write
        btxqs[xctr] << std_to_rsp(data);
        DEBUG_PRINT("[RSG][xctr:%2d][W]: TX response\n", xctr);
        // consume the value from the ctrl:write queue
        b_fwd_rsp[xctr] = rqp_to_rsg_write[xctr].read();
      }
    }
  }
}

/**
 * Task     : I/OHD
 * Purpose  : The IOHD is responsible for the grunt work of the data transfer
 *              related to the requests. The input and output streams
 *              from RQP and to RSG will later be widened based on NRX and NTX,
 *              allowing multiple requests to be parsed in parallel.
 *              THIS PATH MUST BE OPTIMISED FOR PERFORMANCE.
 *
*/
void ihd(tapa::istreams<sb_std_t, SB_NXCTRS>& rqp_to_ihd_read,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& ihd_to_rsg_read,
        tapa::ibuffers<sb_msg_t[SB_MSGS_PER_PAGE], 16, 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& backend_pages) {
        //tapa::ostreams<sb_std_t, SB_NXCTRS>& ihd_to_rsg_read) {
  bool burst_done[SB_NXCTRS] = {0}, rsp_done[SB_NXCTRS] = {0};
  sb_msg_t msgdata[SB_NXCTRS] ={0};
  sb_pageid_t pageid[SB_NXCTRS], nmsgs[SB_NXCTRS], msgs_txed[SB_NXCTRS];
  sb_std_t req[SB_NXCTRS], rsp[SB_NXCTRS];

  // init all variables to assume the control phase
  for(sb_portid_t ixctr = 0; ixctr < SB_NXCTRS; ixctr++)
  {
    burst_done[ixctr] = true;
    rsp_done[ixctr]   = true;
  }

  for(bool valid[SB_NXCTRS];;)
  {
    for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++) // for each xctr queue
    {
    #pragma HLS unroll
      req[xctr] = rqp_to_ihd_read[xctr].peek(valid[xctr]);  // peek the request stream

      //DEBUG_PRINT("[IHD][xctr:%2d][R]: Repeat %d %d %d %d\n", xctr, valid[xctr], req[xctr].c_dn, burst_done[xctr], rsp_done[xctr]);
      if(valid[xctr] && req[xctr].c_dn == 1 && burst_done[xctr] && rsp_done[xctr])
      {
        pageid[xctr] = req[xctr].fields.pageid;     // get pageid
        nmsgs[xctr]  = req[xctr].fields.length;     // get number of messages to read
        rsp[xctr] = req[xctr];                      // store the request data in the response
        msgs_txed[xctr] = 0;                        // set the burst counter to 0
        burst_done[xctr] = false;                   // burst is pending
        rsp_done[xctr] = false;                     // response is pending
        DEBUG_PRINT("[IHD][xctr:%2d][R]: pageid: %d, nmsgs: %d\n", xctr, pageid[xctr], nmsgs[xctr]);
      }
      else if(!(burst_done[xctr]))
      {
        // acquire the buffer for this page
        for(uint8_t dummy = 0; dummy < 1; dummy++)
        {
          DEBUG_PRINT("[IHD][xctr:%2d][R]: Acquiring Buffer\n", xctr); 
          auto section = backend_pages[pageid[xctr]].acquire();
          auto& page_ref = section();
          DEBUG_PRINT("[IHD][xctr:%2d][R]: Starting burst\n", xctr); 
          IHD_DATA_R: for (msgs_txed[xctr] = 0; msgs_txed[xctr] < nmsgs[xctr]; msgs_txed[xctr]++)
          {
            msgdata[xctr] = page_ref[msgs_txed[xctr]];  // TODO: add starting index
            rsp[xctr].std_msg = msgdata[xctr];
            ihd_to_rsg_read[xctr] << rsp[xctr];
            DEBUG_PRINT("[IHD][xctr:%2d][R]: Sent message: %lx\n", xctr, (uint64_t)msgdata[xctr]); 
          }
        }
        burst_done[xctr] = true;                    // mark burst as done
      }
      else if(burst_done[xctr] && !rsp_done[xctr])
      {
        // no explicit response necessary since this is a read
        DEBUG_PRINT("[IHD][xctr:%2d][R]: Sent response: %lx\n", xctr, (uint64_t)msgdata[xctr]); 
        rqp_to_ihd_read[xctr].read();               // consume the packet
        rsp_done[xctr] = true;
      }
      else {}
    }
  }
}

void ohd(tapa::istreams<sb_std_t, SB_NXCTRS>& rqp_to_ohd_write,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& ohd_to_rsg_write,
        tapa::ostreams<uint64_t, 2>& debugtx, 
        tapa::obuffers<sb_msg_t[SB_MSGS_PER_PAGE], 16, 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>>& backend_pages) {
        //tapa::ostreams<sb_std_t, SB_NXCTRS>& ohd_to_rsg_write) { 
  bool burst_done[SB_NXCTRS] = {0}, rsp_done[SB_NXCTRS] = {0};
  sb_msg_t msgdata[SB_NXCTRS] ={0};
  sb_pageid_t pageid[SB_NXCTRS], nmsgs[SB_NXCTRS], msgs_rxed[SB_NXCTRS];
  sb_std_t req[SB_NXCTRS], rsp[SB_NXCTRS];

  // init all variables to assume the control phase
  for(sb_portid_t ixctr = 0; ixctr < SB_NXCTRS; ixctr++)
  {
    burst_done[ixctr] = true;
    rsp_done[ixctr]   = true;
  }

  for(bool valid[SB_NXCTRS];;)
  {
    for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++) // for each xctr queue
    {
    #pragma HLS unroll
      req[xctr] = rqp_to_ohd_write[xctr].peek(valid[xctr]); // peek the request stream

      if(valid[xctr] && req[xctr].c_dn == 1 && burst_done[xctr] && rsp_done[xctr])  // this must be a new request
      {
        // parse how many writes the xctr wants on this page
        pageid[xctr] = req[xctr].fields.pageid;     // get pageid
        nmsgs[xctr]  = req[xctr].fields.length;     // get number of messages to write
        rsp[xctr] = req[xctr];                      // store the request data in the reponse
        msgs_rxed[xctr] = 0;                        // set the burst counter to 0
        burst_done[xctr] = false;                   // burst is pending
        rsp_done[xctr] = false;                     // response is pending
        req[xctr] = rqp_to_ohd_write[xctr].read();  // consume the packet
        debugtx[xctr] << (uint64_t)(0xABCDEFAB);
        DEBUG_PRINT("[OHD][xctr:%2d][W]: pageid: %d, nmsgs: %d\n", xctr, pageid[xctr], nmsgs[xctr]);
      }
      else if(valid[xctr] && req[xctr].c_dn == 0 && !(burst_done[xctr])) // this must be the data packet for the last request
      {
        //debugtx[xctr] << msgdata[xctr];           // debug stuff

        // acquire the buffer for this page
        for(uint8_t dummy = 0; dummy < 1; dummy++)
        {
          auto section = backend_pages[pageid[xctr]].acquire();
          auto& page_ref = section();
          OHD_DATA_W: for (msgs_rxed[xctr] = 0; msgs_rxed[xctr] < nmsgs[xctr]; msgs_rxed[xctr]++)
          {
            req[xctr] = rqp_to_ohd_write[xctr].read();  // consume the packet
            msgdata[xctr] = req[xctr].std_msg;          // extract the message
            debugtx[xctr] << msgs_rxed[xctr];
            // write it in the buffer
            page_ref[msgs_rxed[xctr]] = msgdata[xctr];      // TODO: add starting index          
            DEBUG_PRINT("[OHD][xctr:%2d][W]: Digested message: %lx\n", xctr, (uint64_t)msgdata[xctr]); 
          }
        }
        burst_done[xctr] = true;                    // mark burst as done
      }
      else if(burst_done[xctr] && !rsp_done[xctr])  // generate the response now
      { 
        DEBUG_PRINT("[OHD][xctr:%2d][W]: fwd rsp --> RSG\n", xctr); 
        rsp[xctr].fields.code = SB_RSP_DONE;        // set code
        ohd_to_rsg_write[xctr] << rsp[xctr];        // send response
        rsp_done[xctr] = true;                      // mark response as done
      }
      else {}
    }
  }
}

void debug_task(tapa::istreams<uint64_t, SB_NXCTRS>& debugstreams)
{
  uint64_t msg[SB_NXCTRS] = {0};
  for(bool valid[SB_NXCTRS];;)
  {
    for (int i = 0; i < SB_NXCTRS; i++)
    {
      msg[i] = debugstreams[i].peek(valid[i]);
      if(valid[i])
      {
        debugstreams[i].read();
      }
    }
  }
}


///////////////////////////////////////////////////////////////////////////////
/// TOP-LEVEL SB TASK
///////////////////////////////////////////////////////////////////////////////

// task wrapper that should be invoked at the kernel's top wrapper
void sb_task(tapa::istreams<sb_req_t, SB_NXCTRS>& sb_rxqs,
             tapa::ostreams<sb_rsp_t, SB_NXCTRS>& sb_txqs)
{
  tapa::streams<uint64_t, SB_NXCTRS> debugstreams("debug_streams");
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
  // buffercore_t backend_pages;
  tapa::buffers<sb_msg_t[SB_MSGS_PER_PAGE], 16, 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>> backend_pages;

  tapa::task()
    // .invoke<tapa::detach>(rx_arbiter, (*brxqs_p), (*arbit_rxq_p))
    // .invoke<tapa::detach>(tx_arbiter, (*arbit_txq_p), (*btxqs_p))
    //.invoke<tapa::detach>(loopback,
    //                      sb_rxqs,
    //                      sb_txqs
    //                      );
    .invoke<tapa::detach>(debug_task,
            debugstreams)
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
            ihd_to_rsg_read,
            backend_pages)
            //ihd_to_rsg_read)
    .invoke<tapa::detach>(ohd,
            rqp_to_ohd_write,
            ohd_to_rsg_write,
            debugstreams,
            backend_pages);
            //ohd_to_rsg_write);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//////////////////////
/// KERNEL WRAPPER ///
//////////////////////

void VecAdd(tapa::mmap<const float> vector_a,
            tapa::mmap<const float> vector_b,
            tapa::mmap<float> vector_c) {
  tapa::stream<float> a_q("a");
  tapa::stream<float> b_q("b");
  tapa::stream<float> c_q("c");
  tapa::stream<float> task1_to_task2_pageinfo ("task1_to_task2_pageinfo");
  std::cout << "===" << std::endl;

  tapa::streams<sb_req_t, SB_NXCTRS> sb_rxqs("sb_rxqs");
  tapa::streams<sb_rsp_t, SB_NXCTRS> sb_txqs("sb_txqs");
  
  tapa::task()
    .invoke(Mmap2Stream, vector_a, a_q)
    .invoke(Mmap2Stream, vector_b, b_q)
    .invoke<tapa::detach>(sb_task, sb_rxqs, sb_txqs)
    // .invoke(vadd, a_q, b_q, c_q)
    .invoke(task1, a_q,      sb_rxqs[0], sb_txqs[0], task1_to_task2_pageinfo)
    .invoke(task2, b_q, c_q, sb_rxqs[1], sb_txqs[1], task1_to_task2_pageinfo)
    .invoke(Stream2Mmap, c_q, vector_c);
}

///////////////////////////////////////////////////////////////////////////////
