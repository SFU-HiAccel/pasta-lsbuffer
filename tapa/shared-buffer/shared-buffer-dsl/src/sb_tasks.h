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
        tapa::ostreams<sb_std_t, SB_NXCTRS>& ihd_to_rsg_read,
        ibuffercore_t& backend_pages) {

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
                auto section = backend_pages[pageid].acquire();
                auto& page_ref = section();
                while(nmsgs--)
                {
                    msgdata = page_ref[nmsgs];  // TODO: add starting index
                    readrsp.std_msg = msgdata;
                    ihd_to_rsg_read[xctr] << readrsp;
                }
                // no explicit response necessary since this is a read
            }
        }
    }
}

void ohd(tapa::istreams<sb_std_t, SB_NXCTRS>& rqp_to_ohd_write,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& ohd_to_rsg_write,
        obuffercore_t& backend_pages) {
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
                auto section = backend_pages[pageid].acquire();
                auto& page_ref = section();
                while(nmsgs--)
                {
                    // extract the message
                    req_intrmd = rqp_to_ohd_write[xctr].read();
                    msgdata = req_intrmd.std_msg;
                    // write it in the buffer
                    page_ref[nmsgs] = msgdata;  // TODO: add starting index
                }
                // use the previous request to make the response packet
                req.fields.code = SB_RSP_DONE;
                ohd_to_rsg_write[xctr] << req;
            }
        }
    }
}
