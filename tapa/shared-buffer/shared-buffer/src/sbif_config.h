#ifndef __SBIF_STRUCTS_H__
#define __SBIF_STRUCTS_H__

///////////////////
///  SB CONFIG  ///
///////////////////
#define BYTES_PER_RAMBLOCK  (4)
#define SB_MSGS_PER_PAGE    (16)
using iport_t     = uint8_t;
using oport_t     = uint8_t;
using section_t   = uint16_t;
using addrtype_t  = uint16_t;


///////////////////
/// SBIF CONFIG ///
///////////////////
using sbif_index_t    = uint8_t;
using sbif_depth_t    = uint8_t;
using sbif_pageid_t   = uint16_t;
using sbif_word_t     = uint16_t;
using sbif_line_t     = uint16_t;
using sbif_msg_t      = uint64_t;


/**
 * SBIF 
 * Consider an example where 2 tasks connect to the shared buffer.
 * Task1 can contain 1 TX msg/line and 2 RX msg-lines.
 * Task2 can contain 2 TX msg/lines and 1 RX msg-line.
 * The corresponding sbif on the the shared buffer shall contain:
 *  at least 1 RX msg/line and 2 TX msg/lines for Task1
 *  at least 2 RX msg/lines and 1 TX msg/line for Task2
*/
constexpr sbif_depth_t sbif_depths_rx[3] = {1,2,3};


#endif // __SBIF_STRUCTS_H__