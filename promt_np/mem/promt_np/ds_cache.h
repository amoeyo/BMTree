#ifndef __DESCRIPTOR_CACHE_HH__
#define __DESCRIPTOR_CACHE_HH__

#include "common.h"
#include "statistics.h"
#include "cache.h"

class DS_Cache {
	//public: //需要私有化，public调试用
private:
	//sizeof(Cacheline) == 72字节
	typedef struct Cacheline {
		//从一个物理地址类型中取某些bit，using phy_addr_t = uint64_t
		phy_addr_t __tag : CACHELINE_TAG_BITS;
		phy_addr_t __count : CACHELINE_WAYS_BITS;
		phy_addr_t __line_valid : 1; //__valid重名...
		phy_addr_t __dirty : 1;
		uint8_t __data[CACHELINE];
	} Cacheline_t;
public:

	static DS_Cache& get_ds_instance() {
		static DS_Cache ds_instance;
		return ds_instance;
	}


public:
	DS_Cache(const Cache&) = delete;
	DS_Cache(Cache&&) = delete;
	DS_Cache& operator=(const Cache&) = delete;
	DS_Cache& operator=(Cache&&) = delete;

private:
	DS_Cache();
	~DS_Cache() = default;

public:
	status read(void* addr, void* buffer, size_t n_bytes);

	//默认不写回，不用acc table
	status write(void* addr, void* buffer, size_t n_bytes, bool write_back);

	status clear_cache();

private:

	status __read(phy_addr_t addr, addr_t buffer, size_t n_bytes);

	status __write(phy_addr_t addr, addr_t buffer, size_t n_bytes, bool write_back);

	//刷新缓存
	status hit_fresh(Cacheline_t* cacheline_ways, size_t hit_index);

	//selected_index类似一个寄存器地址，不是访存
	status select_cacheline(Cacheline_t* cacheline_ways, size_t* selected_index);

private:
	Cacheline_t cache[CACHELINES_PER_WAY][CACHELINE_WAYS];
};

/*DS Cache API*/

status read_ds_cache(void* addr, void* buffer, size_t n_bytes);

status write_ds_cache(void* addr, void* buffer, size_t n_bytes, bool write_back = false);

status clear_ds_cache();

#endif