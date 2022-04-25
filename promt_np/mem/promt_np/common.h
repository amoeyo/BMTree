#ifndef __BMTREE_COMMON_HH__
#define __BMTREE_COMMON_HH__

#include <stdint.h>
#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <iostream>

#define PROMT_DEBUG

#define USING_EXTRA_CACHE_FOR_INDEX_TABLE

using phy_addr_t = uint64_t;
using page_addr_t = uint64_t;
using status = int;
using addr_t = uint8_t*;


constexpr uint64_t MEMORY_BITS = 34;//16GB memory
constexpr uint64_t MEMORY_RANGE = (1LLU << MEMORY_BITS); 
constexpr uint64_t SEGMENT_BITS = 27;
constexpr uint64_t SEGMENT = (1U << SEGMENT_BITS); //128M
constexpr uint64_t BACK_SEGMENT_NUM = (MEMORY_RANGE / SEGMENT);
constexpr uint64_t FORE_SEGMENT_NUM = 512; //512????

constexpr size_t CACHELINE_BITS = 6;
constexpr size_t CACHELINE = (1U << CACHELINE_BITS);
constexpr size_t PAGE_BITS = 12;
constexpr size_t PAGE = (1U << PAGE_BITS);

constexpr size_t CTR_OVERFLOW = 16;


constexpr status SUCCESS = 0;
constexpr status FAILURE = 1;
typedef uint8_t cacheline_t[CACHELINE]; // uint8_t*

#define MAKE_MASK(__MASK_LEN, __TYPE) (__TYPE(~(__TYPE(-1) << (__MASK_LEN))))
//check TYPE_BITS(__TYPE) >= (__K)
#define MAKE_ZERO_ERASE(__MASK, __K, __TYPE) (__TYPE(~(__TYPE(__MASK) << (__K))))
#define MAKE_ERASE(__MASK, __K, __UNIT_BITS, __TYPE) (__TYPE(~(__TYPE(__MASK) << (__K * __UNIT_BITS))))

constexpr uint64_t MEMORY_ADDR_MASK = MAKE_MASK(MEMORY_BITS, uint64_t);//16GB memory

#ifdef __linux__
#define memory_aligned_alloc(alignment, size) aligned_alloc(alignment, size)
#define memory_aligned_free(addr) free(addr)
#else
#define memory_aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
#define memory_aligned_free(addr) _aligned_free(addr)
#endif // __linux__


static inline addr_t memory_alloc(size_t size, size_t alignment = CACHELINE) {
	addr_t memory = (addr_t)memory_aligned_alloc(alignment, size);
	if (memory) { memset(memory, 0, size); } // else { assert(0);}
	return memory;
}

static inline addr_t memory_alloc_cacheline(size_t n = 1) {
	return memory_alloc(n * CACHELINE);
}

static inline void memory_free(void* addr) { memory_aligned_free(addr); }

template<class _Ty>
static inline _Ty min_xy(_Ty __x, _Ty __y) {
	return __x < __y ? __x : __y;
}

template<class _Ty>
static inline _Ty max_xy(_Ty __x, _Ty __y) {
	return __x > __y ? __x : __y;
}

#ifdef PROMT_DEBUG
//key, value可以是多级
#define fmt_print(key) do { \
	std::cout << key << std::endl;\
}while(0)

#define fmt_print_kv(key, value) do { \
	std::cout << key << " " << value << std::endl;\
}while(0)

#define set_hex_print() do { \
	std::cout << std::hex; \
}while(0)

#define unset_hex_print() do { \
	std::cout << std::dec; \
}while(0)

//key可以是多级，value必须可以转换为uint64_t
#define fmt_print_k_hex_v(key, value) do { \
	std::cout << key << " 0x" << std::hex << uint64_t(value) << std::dec << std::endl;\
} while(0)

#endif//PROMT_DEBUG

#endif