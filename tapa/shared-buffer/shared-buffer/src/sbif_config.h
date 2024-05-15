#ifndef __SB_CONFIG_H__
#define __SB_CONFIG_H__

///////////////////
///  SB CONFIG  ///
///////////////////

#define SB_NRX  (2)
#define SB_NTX  (2)

#define SB_NUM_PAGES        (64)
#define SB_WORD_SIZE        (4)
#define SB_WORD_SIZE_BITS   (32) // TODO: SBIF_WORD_SIZE << 3
#define SB_PAGE_SIZE        (1024)
#define SB_WORDS_PER_PAGE   (256) // TODO: SB_PAGE_SIZE/SB_WORD_SIZE

using sb_portid_t     = uint8_t;
using sb_pageid_t     = uint16_t;
using sb_stream_t     = ap_uint<SB_WORD_SIZE_BITS>;
using sb_msg_t        = uint64_t;

/**
 * SBIF 
 * Consider an example where 2 tasks connect to the shared buffer.
 * Task1 can contain 1 TX msg/line and 2 RX msg-lines.
 * Task2 can contain 2 TX msg/lines and 1 RX msg-line.
 * The corresponding sbif on the the shared buffer shall contain:
 *  at least 1 RX msg/line and 2 TX msg/lines for Task1
 *  at least 2 RX msg/lines and 1 TX msg/line for Task2
*/
// constexpr sbif_depth_t sbif_depths_rx[3] = {1,2,3};


#endif // __SB_CONFIG_H__