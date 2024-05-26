#include "sbif_config.h"

// converts a sb_req_t to a sb_std_t format
sb_std_t req_to_std(sb_req_t rx_req)
{
    #pragma HLS inline
    sb_std_t tx_std = {0};
    tx_std.c_dn = rx_req.c_dn;
    tx_std.std_msg = rx_req.req_msg;
    return tx_std;
}

// converts a sb_std_t to a sb_rsp_t format
sb_rsp_t std_to_rsp(sb_std_t rx_std)
{
    #pragma HLS inline
    sb_rsp_t tx_rsp = {0};
    tx_rsp.c_dn = rx_std.c_dn;
    tx_rsp.rsp_msg = rx_std.std_msg;
    return tx_rsp;
}


// Request Router
void rqr(tapa::istream<sb_req_t>& rbuf_to_rqr,
        tapa::ostream<sb_std_t>& rqr_to_rqp_grab,
        tapa::ostream<sb_std_t>& rqr_to_rqp_free,
        tapa::ostream<sb_std_t>& rqr_to_rqp_read,
        tapa::ostream<sb_std_t>& rqr_to_rqp_write) {

    for(bool valid;;)
    {
        // peek whether value is available
        sb_req_t req = rbuf_to_rqr.peek(valid);
        bool fwd_rqp_free   = valid && (req.fields.code == SB_REQ_FREE_PAGE);
        bool fwd_rqp_grab   = valid && (req.fields.code == SB_REQ_GRAB_PAGE);
        bool fwd_rqp_read   = valid && (req.fields.code == SB_REQ_READ_MSGS);
        bool fwd_rqp_write  = valid && (req.fields.code == SB_REQ_WRITE_MSGS);

        sb_std_t std_req = req_to_std(req);
        if(fwd_rqp_free)        // force write into the free queue
            rqr_to_rqp_grab << std_req;
        else if(fwd_rqp_grab)   // force write into the grab queue
            rqr_to_rqp_free << std_req;
        else if(fwd_rqp_read)   // force write into the read queue
            rqr_to_rqp_read << std_req;
        else if(fwd_rqp_write)  // force write into the write queue
            rqr_to_rqp_write << std_req;
        else {}
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
        tapa::istream<sb_std_t>& rqr_to_rqp_read,
        tapa::istream<sb_std_t>& rqr_to_rqp_write,
        tapa::ostream<sb_std_t>& rqp_to_pgm_grab,
        tapa::ostream<sb_std_t>& rqp_to_pgm_free,
        tapa::ostream<sb_std_t>& rqp_to_rsg_grab,
        tapa::ostream<sb_std_t>& rqp_to_rsg_free,
        tapa::ostream<sb_std_t>& rqp_to_rsg_read,
        tapa::ostream<sb_std_t>& rqp_to_rsg_write,
        tapa::ostream<sb_std_t>& rqp_to_ihd_read,
        tapa::ostream<sb_std_t>& rqp_to_ohd_write) {

    sb_std_t nb_fwd_req, b_fwd_req;
    // Free > Grab > Read > Write
    for(bool vld_rqr_g, vld_rqr_f, vld_rqr_r, vld_rqr_w;;)
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
        b_fwd_req = rqr_to_rqp_write.peek(vld_rqr_w);
        if(vld_rqr_w)
        {
            // consume ctrl:write message to consume data further
            b_fwd_req = rqr_to_rqp_write.read();
            // get the number of messages in this burst
            sb_pageid_t msgs = b_fwd_req.fields.npages;
            sb_std_t data;
            while(msgs--)
            {
                data = rqr_to_rqp_write.read();  // block for write data
                // data.c_dn = 0;  // must already be a data packet
                rqp_to_ohd_write << data;   // for data
            }
            rqp_to_rsg_write << b_fwd_req;  // for RSG to track
        }

        // non-blocking forwards
        nb_fwd_req = rqr_to_rqp_read.peek(vld_rqr_r);
        if(vld_rqr_r)
        {
            rqp_to_ihd_read << nb_fwd_req;  // for data
            rqp_to_rsg_read << nb_fwd_req;  // for RSG to track
            nb_fwd_req = rqr_to_rqp_read.read();
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
        tapa::istream<sb_std_t>& rqp_to_rsg_read,
        tapa::istream<sb_std_t>& rqp_to_rsg_write,
        tapa::istream<sb_std_t>& ihd_to_rsg_read,
        tapa::istream<sb_std_t>& ohd_to_rsg_write,
        tapa::ostream<sb_rsp_t>& rsg_to_rbuf) {

    // Free > Grab > Read > Write
    sb_std_t nb_fwd_rsp, b_fwd_rsp;
    sb_rsp_t fwd_rsp;
    for(bool vld_rqr_g, vld_rqr_f, vld_rqr_r, vld_rqr_w;;)
    {
        // non-blocking forwards
        nb_fwd_rsp = rqp_to_rsg_free.peek(vld_rqr_f);
        if(vld_rqr_f)   // free queue has some message
        {
            rsg_to_rbuf << std_to_rsp(nb_fwd_rsp);
            b_fwd_rsp = rqp_to_rsg_free.read();
        }
        nb_fwd_rsp = rqp_to_rsg_free.peek(vld_rqr_g);
        if(vld_rqr_g)   // grab queue has some message
        {
            rsg_to_rbuf << std_to_rsp(nb_fwd_rsp);
            b_fwd_rsp = rqp_to_rsg_free.read();
        }

        // blocking forwards
        b_fwd_rsp = rqp_to_rsg_read.peek(vld_rqr_r);
        if(vld_rqr_r)
        {
            // get the number of messages in this burst
            sb_pageid_t msgs = b_fwd_rsp.fields.npages;
            sb_std_t data;
            while(msgs--)
            {
                data = ihd_to_rsg_read.read();  // block for read
                // data.c_dn = 0;  // must already be a data packet
                rsg_to_rbuf << std_to_rsp(data);
            }
            // consume the value from the ctrl:read queue
            b_fwd_rsp = rqp_to_rsg_read.read();
        }
        b_fwd_rsp = rqp_to_rsg_read.peek(vld_rqr_r);
        if(vld_rqr_w)
        {
            // need to wait for write response from OHD
            sb_std_t data;
            data = ohd_to_rsg_write.read();  // block for write
            rsg_to_rbuf << std_to_rsp(data);
            // consume the value from the ctrl:write queue
            b_fwd_rsp = rqp_to_rsg_write.read();
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
void ihd(tapa::istream<sb_std_t>& rqp_to_ihd_read,
        tapa::ostream<sb_std_t>& ihd_to_rsg_read,
        tapa::ibuffer<sb_std_t>& backend_pages) {
}

void ohd(tapa::istream<sb_std_t>& rqp_to_ohd_write,
        tapa::ostream<sb_std_t>& ohd_to_rsg_write,
        tapa::obuffer<sb_std_t>& backend_pages) {
}
