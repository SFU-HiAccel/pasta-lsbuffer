#include "sbif_config.h"

void rqr(tapa::istream<sb_req_t>& rbuf_to_rqp,
        tapa::ostream<sb_std_t>& rqr_to_rqp_grab,
        tapa::ostream<sb_std_t>& rqr_to_rqp_free,
        tapa::ostream<sb_std_t>& rqr_to_rqp_read,
        tapa::ostream<sb_std_t>& rqr_to_rqp_write) {

    sb_req_t request = rbuf_to_rqp.read();
    if(request.fields.code == SB_REQ_GRAB_PAGE)
    {
        rqr_to_rqp_grab << (sb_std_t)request;
    }
    else if(request.fields.code == SB_REQ_FREE_PAGE)
    {
        rqr_to_rqp_free << (sb_std_t)request;
    }
    else if(request.fields.code == SB_REQ_READ_MSGS)
    {
        rqr_to_rqp_read << (sb_std_t)request;
    }
    else if(request.fields.code == SB_REQ_WRITE_MSGS)
    {
        rqr_to_rqp_write << (sb_std_t)request;
    }
    else {}
    

}

void rqp(tapa::istream<sb_std_t>& rbuf_to_rqp,
        tapa::istream<sb_std_t>& pga_to_rqp_sts,
        tapa::istream<sb_std_t>& rqr_to_rqp_grab,
        tapa::istream<sb_std_t>& rqr_to_rqp_free,
        tapa::istream<sb_std_t>& rqr_to_rqp_read,
        tapa::istream<sb_std_t>& rqr_to_rqp_write,
        tapa::ostream<sb_std_t>& rqp_to_pga_grab,
        tapa::ostream<sb_std_t>& rqp_to_pga_free,
        tapa::ostream<sb_std_t>& rqp_to_rsm_grab,
        tapa::ostream<sb_std_t>& rqp_to_rsm_free,
        tapa::ostream<sb_std_t>& rqp_to_ihd_read,
        tapa::ostream<sb_std_t>& rqp_to_ohd_write) {

    // Free > Grab > Read > Write
    if(rqr_to_rqp_free.try_read())
    {
        rqp_to_pga_free << rqr_to_rqp_free.read();
        // wait until free's response can be returned
        rqp_to_rsm_free << pga_to_rqp_free.read();
    }
    if(rqr_to_rqp_grab.try_read())
    {
        rqp_to_pga_grab << rqr_to_rqp_grab.read();
        // wait until grab's response can be returned
        rqp_to_rsm_grab << pga_to_rqp_grab.read();
    }
    if(rqr_to_rqp_read.try_read())
    {
        // forward read requests to ihd
        rqp_to_ihd_read << rqr_to_rqp_read.read();
    }
    if(rqr_to_rqp_write.try_read())
    {
        // forward read requests to ohd
        rqp_to_ohd_write << rqr_to_rqp_write.read();
    }

}


void pga(tapa::istream<sb_std_t>& rqp_to_pga_grab,
        tapa::istream<sb_std_t>& rqp_to_pga_free,
        tapa::ostream<sb_std_t>& pga_to_rqp_sts) {


    if(rqp_to_pga_free.try_read())
    {
        sb_rsp_t rsp;
        rsp.fields.code = SB_RSP_DONE;
        pga_to_rqp_sts << (sb_std_t)rsp;
    }
    if(rqp_to_pga_grab.try_read())
    {
        sb_rsp_t rsp;
        rsp.fields.code = SB_RSP_DONE;
        rsp.fields.pageid = 2;
        pga_to_rqp_sts << (sb_std_t)rsp;
    }
}


void rsg(tapa::istream<sb_std_t>& rqp_to_rsg_grab,
        tapa::istream<sb_std_t>& rqp_to_rsg_free,
        tapa::istream<sb_std_t>& rqp_to_rsg_read,
        tapa::istream<sb_std_t>& rqp_to_rsg_write,
        tapa::istream<sb_std_t>& rqp_to_ihd_read,
        tapa::istream<sb_std_t>& rqp_to_ohd_write,
        tapa::ostream<sb_rsp_t>& rsg_to_rbuf) {

    // Free > Grab > Read > Write
}

void ihd(tapa::istream<sb_std_t>& rqp_to_ihd_read,
        tapa::ostream<sb_std_t>& ihd_to_rsg_read,
        tapa::ibuffer<sb_std_t>& backend_pages) {
}

void ohd(tapa::istream<sb_std_t>& rqp_to_ohd_write,
        tapa::ostream<sb_std_t>& ohd_to_rsg_write,
        tapa::obuffer<sb_std_t>& backend_pages) {
}
