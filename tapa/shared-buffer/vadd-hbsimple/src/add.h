#define TILE 32
#define N 256
#define PACK_LENGTH 2
#define NDBLKS (TILE/PACK_LENGTH)

// using uint32_v2 = tapa::vec_t<uint32_t, 2>;
using float_v2 = tapa::vec_t<float, PACK_LENGTH>;
typedef float data_type;
typedef float_v2 data_type_mmap;

