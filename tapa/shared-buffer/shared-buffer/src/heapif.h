#ifndef __HEAPIF_H__
#define __HEAPIF_H__

#include <stdint.h>

using sbif_index_t    = uint8_t;
using sbif_depth_t    = uint8_t;
using sbif_pageid_t   = uint16_t;
using sbif_word_t     = uint16_t;
using sbif_line_t     = uint16_t;

// all sizes in bytes
#define SBIF_WORD_SIZE  (sizeof(sbif_word_t))
#define SBIF_LINE_SIZE  (sizeof(sbif_line_t))
#define SBIF_MSG_SIZE   (SBIF_LINE_SIZE)

// get the minimum burst length.
// line_size will always be a multiple of word_size.
// Otherwise, the interface logic becomes complex for no reason
#define SBIF_MIN_BURST  ((SBIF_LINE_SIZE/SBIF_WORD_SIZE)

/**
 * Each Heap interface provides two 'ports':
 * Controlone each for read and write. This is done because the routing for the
 * read and write interfaces might require placement in different regions
 * of the PE's floorplan, especially if the PE is involved in heavy
 * dataflow.
 * Keeping the memory port functionality split like this offers two benefits:
 *  1. Allows the PE to have different 'interface heads'
 *  2. A PE that only does one type of I/O operation defines only one port
 *      and hence saves resources.
*/
template <typename msg_t, uint8_t _tx_num, uint8_t _rx_num, sbif_depth_t* _rx_depths, sbif_depth_t* _tx_depths>
class HeapIF {
  private:
    uint8_t tx_num = _tx_num;
    uint8_t rx_num = _rx_num;
    sbif_depth_t tx_depths[tx_num];
    sbif_depth_t rx_depths[rx_num];

  public:
    /**
      * HI Control Port:
      *   contains the control information.
      *   TX:
      *     write_readn - determines whether the request is read or write.
      *     page_id     - this is the pointer ID that would have been shared.
      *     line_offset - which line to view in the page.
      *     num_lines   - how many lines (messages) to read
      *   RX:
      *     ack         - acknowledgement from peer TX that a message is read.
      *     sb_response - response from the SB about malloc/free requests
      *
      * @todo:
      *   * Let the user instantiate the entire struct, but warn about unused
      *     RX/TX ports if the task is only producing/consuming.
      */
    
    typedef struct
    {
      bool          write_readn;
      sbif_pageid_t page_id;
      sbif_index_t  line_offset;
      uint8_t       num_lines;
    }ctx_t;
    typedef struct
    {
      bool          ack;
      sbif_word_t   sb_response;
    }crx_t;
    typedef struct
    {
      ctx_t ctx;
      crx_t crx;
    }sbif_ctrl_t;

    /**
      * HI Data Port:
      *   create the packed structures for data streams
      *   TX and RX queues are instantiated based on 
      */
    typedef struct
    {
      msg_t data;
    }sbif_dtx_t;
    typedef struct
    {
      msg_t data;
    }sbif_drx_t;
    

  HeapIF() {
    std::cout << "init heap i/f" << std::endl;
    std::cout << "TX Queues: " << i << std::endl;
    for(uint8_t i; i < tx_num; i++)
    {
      std::cout << tx_depths[i] << ", ";
    }
    std::cout << std::endl;
    std::cout << "RX Queues: " << i << std::endl;
    for(uint8_t i; i < rx_num; i++)
    {
      std::cout << rx_depths[i] << ", ";
    }
    std::cout << std::endl;
  }

};


#endif // __HEAPIF_H__
