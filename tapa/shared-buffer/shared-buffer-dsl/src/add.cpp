#include <tapa.h>
#include "add.h"
#include "brahma.h"


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
  sb_req_t request = {0};
  request.c_dn = 1;
  request.fields.code = SB_REQ_GRAB_PAGE;
  request.fields.npages = 1;
  tx_task1_to_sb << request;

  // now wait for the response to be received
  sb_rsp_t rsp;
  rsp = rx_sb_to_task1.read();
  #ifndef __SYNTHESIS__
  printf("response message: %lu %lu\n", (uint64_t)rsp.fields.code, (uint64_t)rsp.fields.npages);
  #endif
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
}

///////////////////////////////////////////////////////////////////////////////
///                SHARED BUFFER SUBTASKS AND GORY DETAILS                  ///
///////////////////////////////////////////////////////////////////////////////


void loopback(tapa::istreams<sb_req_t, SB_NXCTRS>& brxqs,
              tapa::ostreams<sb_rsp_t, SB_NXCTRS>& btxqs)
{
    for(bool valid[SB_NXCTRS];;)
    {
        for(uint8_t xctr = 0; xctr < SB_NXCTRS; xctr++) // this check is being done for each xctr stream being rxed
        {
        #pragma HLS unroll    // full unroll by a factor of SB_NXCTRS
            sb_req_t req = brxqs[xctr].peek(valid[xctr]);
            if(valid[xctr])
            {
                btxqs[xctr] << req_to_rsp(req);
                req = brxqs[xctr].read();
            }
        }
    }
}

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

// Request Router
void rqr(tapa::istreams<sb_req_t, SB_NXCTRS>& brxqs,
        tapa::ostream<sb_std_t>& rqr_to_rqp_grab,
        tapa::ostream<sb_std_t>& rqr_to_rqp_free,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& rqr_to_rqp_read,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& rqr_to_rqp_write) {

    for(bool valid[SB_NXCTRS];;)
    {
        for(uint8_t xctr = 0; xctr < SB_NXCTRS; xctr++) // this check is being done for each xctr stream being rxed
        {
        #pragma HLS unroll    // full unroll by a factor of SB_NXCTRS
            // peek whether value is available
            sb_req_t req = brxqs[xctr].peek(valid[xctr]);   // TODO: is this OK? Or does `req` need to be an array for the loop to be unrolled?
            bool fwd_rqp_free   = valid[xctr] && (req.fields.code == SB_REQ_FREE_PAGE);
            bool fwd_rqp_grab   = valid[xctr] && (req.fields.code == SB_REQ_GRAB_PAGE);
            bool fwd_rqp_read   = valid[xctr] && (req.fields.code == SB_REQ_READ_MSGS);
            bool fwd_rqp_write  = valid[xctr] && (req.fields.code == SB_REQ_WRITE_MSGS);

            sb_std_t std_req = req_to_std(req);
            if(fwd_rqp_free)        // force write into the free queue
                rqr_to_rqp_grab         << std_req;
            else if(fwd_rqp_grab)   // force write into the grab queue
                rqr_to_rqp_free         << std_req;
            else if(fwd_rqp_read)   // force write into the read queue
                rqr_to_rqp_read[xctr]   << std_req;
            else if(fwd_rqp_write)  // force write into the write queue
                rqr_to_rqp_write[xctr]  << std_req;
            else {}
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

    sb_std_t b_fwd_req;
    // Free > Grab > Read > Write
    for(bool vld_rqr_g, vld_rqr_f;;)
    {
        // blocking forwards
        b_fwd_req = rqr_to_rqp_free.peek(vld_rqr_f);
        if(vld_rqr_f)   // free queue has some message
        {
            rqp_to_pgm_free << rqr_to_rqp_free.read();
            // hard wait on pgm:status queue
            rqp_to_rsg_free << pgm_to_rqp_sts.read();
        }
        b_fwd_req = rqr_to_rqp_grab.peek(vld_rqr_g);
        if(vld_rqr_g)   // grab queue has some message
        {
            rqp_to_pgm_grab << rqr_to_rqp_grab.read();
            // hard wait on pgm:status queue
            rqp_to_rsg_grab << pgm_to_rqp_sts.read();
        }
    }

    // WRITE: expecting this to be unrolled and done in parallel with free/grab
    for(bool vld_rqr_w[SB_NXCTRS];;)
    {
        for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
        {
        #pragma HLS unroll
            sb_std_t nb_fwd_req = rqr_to_rqp_write[xctr].peek(vld_rqr_w[xctr]);
            if(vld_rqr_w[xctr])
            {
                // consume ctrl:write message to consume data further
                nb_fwd_req = rqr_to_rqp_write[xctr].read();
                // get the number of messages in this burst
                sb_pageid_t msgs = nb_fwd_req.fields.npages;  // TODO: doesn't look like `msgs` or `nb_fwd_req`  need to be created as an array because it is internal to the loop
                sb_std_t data;
                while(msgs--)
                {
                    data = rqr_to_rqp_write[xctr].read();  // block for write data
                    // data.c_dn = 0;  // must already be a data packet
                    rqp_to_ohd_write[xctr] << data;   // for data
                }
                rqp_to_rsg_write[xctr] << nb_fwd_req;  // for RSG to track
            }
        }
    }

    // READ: expecting this to be unrolled and done in parallel with free/grab
    for(bool vld_rqr_r[SB_NXCTRS];;)
    {
        for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
        {
        #pragma HLS unroll
            // non-blocking forwards
            sb_std_t nb_fwd_req = rqr_to_rqp_read[xctr].peek(vld_rqr_r[xctr]);
            if(vld_rqr_r[xctr])
            {
                rqp_to_ihd_read[xctr] << nb_fwd_req;  // for data
                rqp_to_rsg_read[xctr] << nb_fwd_req;  // for RSG to track
                nb_fwd_req = rqr_to_rqp_read[xctr].read();
            }
        }
    }
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

    sb_std_t fwd_rsp;
    for(bool vld_g, vld_f;;)
    {
        // handle page deallocation
        fwd_rsp = rqp_to_pgm_free.peek(vld_f);
        if(vld_f)
        {
            sb_std_t rsp;
            rsp.fields.code = SB_RSP_DONE;
            pgm_to_rqp_sts << rsp;
        }

        // handle page allocation
        fwd_rsp = rqp_to_pgm_grab.peek(vld_g);
        if(vld_g)
        {
            sb_std_t rsp;
            rsp.fields.code = SB_RSP_DONE;
            // hardcoded to return pageid 2
            rsp.fields.pageid = 2;
            pgm_to_rqp_sts << rsp;
        }
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
    sb_std_t nb_fwd_rsp, b_fwd_rsp;
    sb_rsp_t fwd_rsp;
    for(bool vld_rqr_g, vld_rqr_f;;)
    {
        // TODO: merge from grab/free to respective queue
        // non-blocking forwards
        nb_fwd_rsp = rqp_to_rsg_free.peek(vld_rqr_f);
        if(vld_rqr_f)   // free queue has some message
        {
            btxqs[0] << std_to_rsp(nb_fwd_rsp);
            b_fwd_rsp = rqp_to_rsg_free.read();
        }
        nb_fwd_rsp = rqp_to_rsg_free.peek(vld_rqr_g);
        if(vld_rqr_g)   // grab queue has some message
        {
            btxqs[0] << std_to_rsp(nb_fwd_rsp);
            b_fwd_rsp = rqp_to_rsg_free.read();
        }
    }

    for(bool vld_rqr_r[SB_NXCTRS];;)
    {
        for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
        {
        #pragma HLS unroll
            // blocking forwards
            b_fwd_rsp = rqp_to_rsg_read[xctr].peek(vld_rqr_r[xctr]);
            if(vld_rqr_r[xctr])
            {
                // get the number of messages in this burst
                sb_pageid_t msgs = b_fwd_rsp.fields.npages;
                sb_std_t data;
                while(msgs--)
                {
                    data = ihd_to_rsg_read[xctr].read();  // block for read
                    // data.c_dn = 0;  // must already be a data packet
                    btxqs[xctr] << std_to_rsp(data);
                }
                // consume the value from the ctrl:read queue
                b_fwd_rsp = rqp_to_rsg_read[xctr].read();
            }
        }
    }
    for(bool vld_rqr_w[SB_NXCTRS];;)
    {
        for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
        {
        #pragma HLS unroll
            b_fwd_rsp = rqp_to_rsg_write[xctr].peek(vld_rqr_w[xctr]);
            if(vld_rqr_w[xctr])
            {
                // need to wait for write response from OHD
                sb_std_t data;
                data = ohd_to_rsg_write[xctr].read();  // block for write
                btxqs[xctr] << std_to_rsp(data);
                // consume the value from the ctrl:write queue
                b_fwd_rsp = rqp_to_rsg_write[xctr].read();
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
        //tapa::ostreams<sb_std_t, SB_NXCTRS>& ihd_to_rsg_read,
        //ibuffercore_t& backend_pages) {
        tapa::ostreams<sb_std_t, SB_NXCTRS>& ihd_to_rsg_read) {

    for(bool valid[SB_NXCTRS];;)
    {
        for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++) // for each xctr queue
        {
        #pragma HLS unroll
            // parse how many reads the xctr wants on this page
            sb_std_t req = rqp_to_ihd_read[xctr].peek(valid[xctr]);
            sb_pageid_t pageid, nmsgs;
            sb_msg_t msgdata = {0};
            sb_std_t readrsp = {0};
            if(valid[xctr] && req.c_dn == 1)
            {
                pageid = req.fields.pageid;     // get pageid
                nmsgs  = req.fields.npages;     // get number of messages to read
                readrsp.c_dn = 0;               // define control packet
                // acquire the buffer for this page
                // auto section = backend_pages[pageid].acquire();
                // auto& page_ref = section();
                while(nmsgs--)
                {
                    msgdata = 0xDEADBEEF;//page_ref[nmsgs];  // TODO: add starting index
                    readrsp.std_msg = msgdata;
                    ihd_to_rsg_read[xctr] << readrsp;
                }
                // no explicit response necessary since this is a read
            }
        }
    }
}

void ohd(tapa::istreams<sb_std_t, SB_NXCTRS>& rqp_to_ohd_write,
        //tapa::ostreams<sb_std_t, SB_NXCTRS>& ohd_to_rsg_write,
        //obuffercore_t& backend_pages) {
        tapa::ostreams<sb_std_t, SB_NXCTRS>& ohd_to_rsg_write) {
    for(bool valid[SB_NXCTRS];;)
    {
        for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++) // for each xctr queue
        {
        #pragma HLS unroll
            // parse how many writes the xctr wants on this page
            sb_std_t req = rqp_to_ohd_write[xctr].peek(valid[xctr]);
            sb_std_t req_intrmd;
            sb_pageid_t pageid, nmsgs;
            sb_msg_t msgdata ={0};
            if(valid[xctr] && req.c_dn == 1)
            {
                // consume control packet
                req = rqp_to_ohd_write[xctr].read();
                pageid = req.fields.pageid;     // get pageid
                nmsgs  = req.fields.npages;     // get number of messages to write
                // acquire the buffer for this page
                //auto section = backend_pages[pageid].acquire();
                //auto& page_ref = section();
                while(nmsgs--)
                {
                    // extract the message
                    req_intrmd = rqp_to_ohd_write[xctr].read();
                    msgdata = req_intrmd.std_msg;
                    // write it in the buffer
                    //page_ref[nmsgs] = msgdata;  // TODO: add starting index
                }
                // use the previous request to make the response packet
                req.fields.code = SB_RSP_DONE;
                ohd_to_rsg_write[xctr] << req;
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
  buffercore_t backend_pages;

  tapa::task()
    // .invoke<tapa::detach>(rx_arbiter, (*brxqs_p), (*arbit_rxq_p))
    // .invoke<tapa::detach>(tx_arbiter, (*arbit_txq_p), (*btxqs_p))
    // .invoke<tapa::detach>(loopback,
    //                       sb_rxqs,
    //                       sb_txqs
    //                       )
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
    .invoke(sb_task, sb_rxqs, sb_txqs)
    // .invoke(vadd, a_q, b_q, c_q)
    .invoke(task1, a_q,      sb_rxqs[0], sb_txqs[0], task1_to_task2_pageinfo)
    .invoke(task2, b_q, c_q, sb_rxqs[1], sb_txqs[1], task1_to_task2_pageinfo)
    .invoke(Stream2Mmap, c_q, vector_c);
}

///////////////////////////////////////////////////////////////////////////////