#ifndef __SB_INLINE_FUNCS_H__
#define __SB_INLINE_FUNCS_H__

#include "sb_config.h"

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

sb_rsp_t req_to_rsp(sb_req_t rx_std)
{
    #pragma HLS inline
    sb_rsp_t tx_rsp = {0};
    tx_rsp.c_dn = rx_std.c_dn;
    tx_rsp.rsp_msg = rx_std.req_msg;
    return tx_rsp;
}

#endif //__SB_INLINE_FUNCS_H__