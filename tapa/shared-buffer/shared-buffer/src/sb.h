#ifndef __SB_H__
#define __SB_H__
#include <stdint.h>
#include <iostream>
#include "sbif_config.h"
#include "sbif.h"

/**
 * datatype_t --> type of the data packet (page) that will be ID-ed.
 *                All headers and bookkeeping is appended within sharedBuffer.
 * iports     --> Number of input ports (producers)
 * oports     --> Number of output ports (consumers)
 * npages     --> Total number of buffer-blocks (pages) to maintain
 * concurrency--> Number of concurrent operations to allow.
 *                concurrency <= (iports+oports)
*/
template <typename sb_msg_t,  iport_t iports, oport_t oports, section_t npages, section_t concurrency>
class SharedBuffer {
private:
  // size of container (number of RAM blocks) required to store 1 `datatype_t`
  size_t container_size;
  // declare buffer pointer as a void type,
  // dimension information is stored in the public variable `buffercore_t`
  void* buffercore_p;
  void* rxdstreams_p;
  void* txdstreams_p;
public:
  // first abstract the buffercore type
  using buffercore_t = tapa::buffer<sb_msg_t[npages], concurrency, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
  using rxdstreams_t = tapa::streams<sb_msg_t, iports>;
  using txdstreams_t = tapa::streams<sb_msg_t, oports>;
  using rxastreams_t = tapa::streams<addrtype_t, iports>;
  using txastreams_t = tapa::streams<addrtype_t, oports>;

  // constructor
  SharedBuffer() {
    container_size = sizeof(sb_msg_t)/BYTES_PER_RAMBLOCK;

    // create temporary pointers to the buffercore and stream-arrays
    buffercore_t* buffercore_p_ = new buffercore_t;
    rxdstreams_t* rxdstreams_p_ = new rxdstreams_t;// rxds("rxdstreams");
    txdstreams_t* txdstreams_p_ = new txdstreams_t;// txds("txdstreams");
    // cast this pointer type to the corresponding private pointers
    buffercore_p = static_cast<void*>(buffercore_p_);
    rxdstreams_p = static_cast<void*>(rxdstreams_p_);
    txdstreams_p = static_cast<void*>(txdstreams_p_);

    // tapa::streams<datatype_t, iports> rx("rxstream");
    // tapa::streams<datatype_t, oports> tx("txstream");
    validate_config();
  }

  ~SharedBuffer() {
    // delete buffercore pointer upon destruction
    delete static_cast<buffercore_t*>(buffercore_p);
    delete static_cast<rxdstreams_t*>(rxdstreams_p);
    delete static_cast<txdstreams_t*>(txdstreams_p);
  }

  /**
   * validate the configuration of the buffer. Expected to be a bunch of
   * static assertions with some helpful messages.
   * This function breaks compilation on an error.
   * */ 
  bool validate_config()
  {
    // print some info first
    std::cout << "shared buff   " << std::endl;
    std::cout << "i/p ports   : " << (unsigned)iports << std::endl;
    std::cout << "o/p ports   : " << (unsigned)oports << std::endl;
    std::cout << "concurrency : " << concurrency << std::endl;
    std::cout << "typesize    : " << sizeof(sb_msg_t) << std::endl;
    std::cout << "totalsize   : " << sizeof(buffercore_t) << std::endl;

    static_assert(concurrency <= (iports + oports),\
    "Cannot provide concurrency more than total i/o-ports");
  }

  size_t get_container_size() const {
    return container_size;
  }

  bool connect(void* _sbif_p, sbif_depth_t _tx_depth, sbif_depth_t _rx_depth)
  {
    using SBIF_t = SBIF<sbif_msg_t, _tx_depth, _rx_depth>;
    SBIF_t* sbif_p = static_cast<SBIF_t*>(_sbif_p);
    auto temp = sbif_p->sbif_data_p->data;
    using newType =  decltype(temp);
    SBIF<newType, sbif_p->get_tx_depth(), sbif_p->get_rx_depth()> sbifSB1;
    // tx_msgs = _sbif->get_rx_depth();
  }
};

#endif // __SB_H__
