#define TILE 32
#define N 256
#define PACK_LENGTH 2
#define NDBLKS (TILE/PACK_LENGTH)

#include <ap_int.h>

template <typename T>
using bits = ap_uint<tapa::widthof<T>()>;

// using uint32_v2 = tapa::vec_t<uint32_t, 2>;
using float_v2 = tapa::vec_t<float, PACK_LENGTH>;
using data_type = float;
using data_type_mmap = float_v2;

void VecAdd(tapa::mmap<bits<data_type_mmap>> vector_a,
            tapa::mmap<bits<data_type_mmap>> vector_b,
            tapa::mmap<bits<data_type_mmap>> vector_c, uint64_t n_tiles);
