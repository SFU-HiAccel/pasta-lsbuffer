#ifndef __SB_H__
#define __SB_H__
#include <stdint.h>
#include <iostream>
#include "tapa.h"
#include "sbif_config.h"
#include "sb_tasks.h"

void loopback(tapa::istreams<sb_req_t, SB_NXCTRS>& brxqs,
              tapa::ostreams<sb_rsp_t, SB_NXCTRS>& btxqs);

// task wrapper that should be invoked at the kernel's top wrapper
void sb_task(tapa::istreams<sb_req_t, SB_NXCTRS>& sb_rxqs,
             tapa::ostreams<sb_rsp_t, SB_NXCTRS>& sb_txqs);

#endif //__SB_H__