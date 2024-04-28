#ifndef __SB_H__
#define __SB_H__
#include <stdint.h>
#include <iostream>

#define BYTES_PER_RAMBLOCK (4)

using iport_t = uint8_t;
using oport_t = uint8_t;
using section_t = uint16_t;
using addrtype_t = uint16_t;

/**
 * datatype_t --> type of the data packet to be interchanged.
 *                All headers and bookkeeping is appended within sharedBuffer.
 * iports     --> Number of input ports (producers)
 * oports     --> Number of output ports (consumers)
 * nblocks    --> Total number of buffer-blocks to maintain
 * concurrency--> Number of concurrent operations to allow.
 *                concurrency <= (iports+oports)
*/
template <typename datatype_t, iport_t iports, oport_t oports, section_t nblocks, section_t concurrency>
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
  using buffercore_t = tapa::buffer<datatype_t[nblocks], concurrency, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
  using rxdstreams_t = tapa::streams<datatype_t, iports>;
  using txdstreams_t = tapa::streams<datatype_t, oports>;
  using rxastreams_t = tapa::streams<addrtype_t, iports>;
  using txastreams_t = tapa::streams<addrtype_t, oports>;

  // constructor
  SharedBuffer() {
    std::cout << "init a shared buff"<< std::endl;
    std::cout << "i/p ports   : " << iports << std::endl;
    std::cout << "o/p ports   : " << oports << std::endl;
    std::cout << "concurrency : " << concurrency << std::endl;
    std::cout << "typesize    : " << sizeof(datatype_t) << std::endl;
    std::cout << "totalsize   : " << sizeof(buffercore_t) << std::endl;
    container_size = sizeof(datatype_t)/BYTES_PER_RAMBLOCK;

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
    static_assert(concurrency < (iports + oports),\
    "Cannot provide concurrency more than total i/o-ports");
  }

  size_t get_container_size() const {
    return container_size;
  }
};

#endif // __SB_H__
