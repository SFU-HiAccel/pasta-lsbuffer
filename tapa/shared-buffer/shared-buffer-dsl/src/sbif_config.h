#ifndef __SB_CONFIG_H__
#define __SB_CONFIG_H__

///////////////////
///  SB CONFIG  ///
///////////////////

#define SB_NXCTRS (2)
#define SB_NRX    (SB_NXCTRS)
#define SB_NTX    (SB_NXCTRS)

#define SB_NUM_PAGES        (64)
#define SB_WORD_SIZE        (4)
#define SB_WORD_SIZE_BITS   (32)    // TODO: SBIF_WORD_SIZE << 3
#define SB_PAGE_SIZE        (1024)
#define SB_MSGS_PER_PAGE    (256)   // TODO: SB_PAGE_SIZE/SB_WORD_SIZE

using sb_portid_t     = uint8_t;
using sb_pageid_t     = uint16_t;
using sb_stream_t     = ap_uint<SB_WORD_SIZE_BITS>;
using sb_msg_t        = uint64_t;

#define SB_REQ_WRITE_MSGS (0x1)
#define SB_REQ_READ_MSGS  (0x2)
#define SB_REQ_GRAB_PAGE  (0x4)
#define SB_REQ_FREE_PAGE  (0x8)

#define SB_RSP_WAIT   (0x1)
#define SB_RSP_DONE   (0x2)
#define SB_RSP_FAIL   (0x4)

/**
 * Standard Request Type:
 * 
 *  <-----------------------sb_msg_t---------------------->
 * |           |  1   |      8     |      8      |    8    |
 *  <-padding-> <c_dn> <-req_code-> <-num_pages-> <-pg_idx->
 * 
*/
typedef struct {
  union{
    struct{
      sb_pageid_t pageid;
      sb_pageid_t npages;
      uint8_t     code;
    }fields;
    sb_msg_t req_msg;
  };
  bool c_dn;
}sb_req_t;

/**
 * Standard Response Type:
 * 
 *  <-----------------------sb_msg_t---------------------->
 * |           |  1   |      8     |      8      |    8    |
 *  <-padding-> <c_dn> <-req_code-> <-num_pages-> <-pg_idx->
 * 
*/
typedef struct {
  union{
    struct{
      sb_pageid_t pageid;
      sb_pageid_t npages;
      uint8_t     code;
    }fields;
    sb_msg_t rsp_msg;
  };
  bool c_dn;
}sb_rsp_t;


typedef struct {
  union{
    struct{
      sb_pageid_t pageid;
      sb_pageid_t npages;
      uint8_t     code;
    }fields;
    sb_msg_t std_msg;
  };
  bool c_dn;
}sb_std_t;


/**
 * datatype_t --> type of the data packet (page) that will be ID-ed.
 *                All headers and bookkeeping is appended within sharedBuffer.
 * SB_NRX     --> Number of input ports (producers)
 * SB_NTX     --> Number of output ports (consumers)
 * npages     --> Total number of buffer-blocks (pages) to maintain
 * concurrency--> Number of concurrent operations to allow.
 *                concurrency <= (SB_NRX+SB_NTX)
*/
typedef struct
{
  union{
    uint8_t valid : 1;
    uint8_t pad   : 7;
  }header;
  sb_pageid_t pageid;
}sb_metadata_t;

sb_metadata_t metadata_t[SB_NUM_PAGES] = {0};
bool valid_pages[8][(SB_NUM_PAGES>>3)] = {0};

// used to set pageids in metadata when that page is requested for the first time
sb_pageid_t pageid_init_counter = 0;

uint8_t arbit_rx;
uint8_t arbit_tx;
bool tx_available; // must update with notif counter checking  

// declare all buffer types
using buffercore_t  = tapa::buffers<sb_msg_t[SB_MSGS_PER_PAGE], SB_NUM_PAGES, 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
using ibuffercore_t = tapa::ibuffers<sb_msg_t[SB_MSGS_PER_PAGE], SB_NUM_PAGES, 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
using obuffercore_t = tapa::obuffers<sb_msg_t[SB_MSGS_PER_PAGE], SB_NUM_PAGES, 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
// declare all stream types
using brxqs_t  = tapa::streams<sb_req_t, SB_NXCTRS>;
using btxqs_t  = tapa::streams<sb_rsp_t, SB_NXCTRS>;
using brxq_t   = tapa::stream<sb_req_t>;
using btxq_t   = tapa::stream<sb_rsp_t>;
brxqs_t sb_rxqs("sb_rxqs");
btxqs_t sb_txqs("sb_txqs");
// brxqs_t* brxqs_p;// = nullptr;
// btxqs_t* btxqs_p;// = nullptr;
// brxq_t* arbit_rxq_p;// = nullptr;
// btxq_t* arbit_txq_p;// = nullptr;


#endif // __SB_CONFIG_H__
