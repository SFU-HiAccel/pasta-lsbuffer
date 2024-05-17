#define TILE    (32)
#define N       (32)
#define DEPTH   (2)
#define PACK_LENGTH 2
#define NDBLKS (TILE/PACK_LENGTH)

#include <ap_int.h>

template <typename T>
using bits = ap_uint<tapa::widthof<T>()>;

// using uint32_v2 = tapa::vec_t<uint32_t, 2>;
using float_v2 = tapa::vec_t<float, PACK_LENGTH>;
using data_type = float;
using data_type_mmap = float_v2;
using msg_t = data_type_mmap;

void VecAdd(tapa::mmap<const float> vector_a,
            tapa::mmap<const float> vector_b,
            tapa::mmap<float> vector_c);
