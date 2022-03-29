#include "ds_cache.h"

/*inline tool func*/
static inline void clear_cacheline_register(cacheline_t cacheline_reg) {
	memset(cacheline_reg, 0, CACHELINE);
}

static inline void cache_read_statistics(bool cacheline_hit) {
	if (cacheline_hit) {
		g_Statistics.inc_extra_cache_hit_cntr();
	}
	else {
		g_Statistics.inc_extra_cache_miss_cntr();
	}
}

static inline void cache_write_statistics(bool cacheline_hit) {
	if (cacheline_hit) {
		g_Statistics.inc_extra_cache_hit_cntr();
	}
	else {
		g_Statistics.inc_extra_cache_miss_cntr();
	}
}



//class Cache definition

DS_Cache::DS_Cache() {
	memset(cache, 0, sizeof(cache));
}

status
DS_Cache::read(void* addr, void* buffer, size_t n_bytes) {
	if (!addr || !buffer || !n_bytes) return PARAMETER_ERROR; //中途的异常检查省略
	//if (n_bytes > ADDR_LEN) return PARAMETER_ERROR;
	return __read((phy_addr_t)addr, (addr_t)buffer, n_bytes);
}

//默认不写回，不用acc table
status
DS_Cache::write(void* addr, void* buffer, size_t n_bytes, bool write_back) {
	if (!addr || !buffer || !n_bytes) return PARAMETER_ERROR; //中途的异常检查省略
	//if (n_bytes > ADDR_LEN) return PARAMETER_ERROR;
	return __write((phy_addr_t)addr, (addr_t)buffer, n_bytes, write_back);
}

status
DS_Cache::clear_cache() {
	memset(cache, 0, sizeof(cache));
	return SUCCESS;
}

status
DS_Cache::__read(phy_addr_t addr, addr_t buffer, size_t n_bytes) {
	// addr -> buffer
	phy_addr_t tag = __parse_tag(addr);
	phy_addr_t group_index = __parse_grp_idx(addr);
	size_t bytes_index = size_t(__parse_bytes_idx(addr));

	Cacheline_t* cacheline_ways = cache[group_index];
	size_t lru_eviction_index = 0;
	size_t __n_bytes = min_xy(CACHELINE - bytes_index, n_bytes); //可能跨行

	bool cacheline_hit = false;

	for (size_t idx = 0; idx < CACHELINE_WAYS; idx++) {
		if (cacheline_ways[idx].__line_valid == VALID && cacheline_ways[idx].__tag == tag) {
			lru_eviction_index = idx; //之后调整所有比这个条目计数器小的计数器
			cacheline_hit = true; //命中直接继续处理
			break;
		}
	}
	cache_read_statistics(cacheline_hit);
	//没有命中，先选一个空位，将数据调上来后再处理
	if (!cacheline_hit) {
		select_cacheline(cacheline_ways, &lru_eviction_index); //选中的条目计数器会被调至最大，用来更新其他条目计数器
		if (cacheline_ways[lru_eviction_index].__dirty == DIRTY) {//访存刷回
			phy_addr_t __tag = cacheline_ways[lru_eviction_index].__tag;
			phy_addr_t __addr = __comb_cacheline_addr(__tag, group_index);
			memory_data_write((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);//写回NVM
			cacheline_ways[lru_eviction_index].__dirty = CLEAN;
		}
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_read((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE); //从NVM读对应的counter节点
		cacheline_ways[lru_eviction_index].__tag = tag;
	}

	cache_data_move(buffer, cacheline_ways[lru_eviction_index].__data + bytes_index, __n_bytes);
	hit_fresh(cacheline_ways, lru_eviction_index); //调整计数器

	status other_hit = true;
	if (__n_bytes < n_bytes) { //越界了
		other_hit = __read(addr + __n_bytes, buffer + __n_bytes, n_bytes - __n_bytes);
	}

	return (cacheline_hit && other_hit) ? CACHE_HIT : CACHE_MISS;
}

status
DS_Cache::__write(phy_addr_t addr, addr_t buffer, size_t n_bytes, bool write_back) {
	// buffer -> addr
	phy_addr_t tag = __parse_tag(addr);
	phy_addr_t group_index = __parse_grp_idx(addr);
	size_t bytes_index = size_t(__parse_bytes_idx(addr));

	Cacheline_t* cacheline_ways = cache[group_index];
	size_t lru_eviction_index = 0;
	size_t __n_bytes = min_xy(CACHELINE - bytes_index, n_bytes);

	bool cacheline_hit = false;
	for (size_t idx = 0; idx < CACHELINE_WAYS; idx++) {
		if (cacheline_ways[idx].__line_valid == VALID && cacheline_ways[idx].__tag == tag) {
			lru_eviction_index = idx; //之后调整所有比这个条目计数器小的计数器
			cacheline_hit = true; //命中直接继续处理
			break;
		}
	}
	cache_write_statistics(cacheline_hit);
	//第一次被更新的时候需要记录一些东西
	if (!cacheline_hit) {
		select_cacheline(cacheline_ways, &lru_eviction_index);
		if (cacheline_ways[lru_eviction_index].__dirty == DIRTY) {//访存刷回
			phy_addr_t __tag = cacheline_ways[lru_eviction_index].__tag;
			phy_addr_t __addr = __comb_cacheline_addr(__tag, group_index);
			memory_data_write((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);
		}
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_read((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);//将页提取上来
		cacheline_ways[lru_eviction_index].__tag = tag;
	}

	cache_data_move(cacheline_ways[lru_eviction_index].__data + bytes_index, buffer, __n_bytes);
	hit_fresh(cacheline_ways, lru_eviction_index); //调整计数器

	if (write_back) {
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_write((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);
	}
	cacheline_ways[lru_eviction_index].__dirty = (write_back ? CLEAN : DIRTY);

	status other_hit = true;
	if (__n_bytes < n_bytes) { //越界了
		other_hit = __write(addr + __n_bytes, buffer + __n_bytes, n_bytes - __n_bytes, write_back);
	}

	return (cacheline_hit && other_hit) ? CACHE_HIT : CACHE_MISS;
}

//刷新规则：
	/// @brief : 空条目或被驱逐条目的计数器会被调整至最大
	/// @brief : 比被选中的条目计数器小的有效计数器会自增，之后选中条目计数器置0

status
DS_Cache::hit_fresh(Cacheline_t* cacheline_ways, size_t hit_index) {
	phy_addr_t __count = cacheline_ways[hit_index].__count;
	for (size_t i = 0; i < CACHELINE_WAYS; i++) {
		if (cacheline_ways[i].__line_valid == VALID
			&& cacheline_ways[i].__count < __count) {
			++cacheline_ways[i].__count;
		}
	}
	cacheline_ways[hit_index].__count = 0;
	return SUCCESS;
}

//selected_index类似一个寄存器地址，不是访存
status
DS_Cache::select_cacheline(Cacheline_t* cacheline_ways, size_t* selected_index) {
	for (size_t i = 0; i < CACHELINE_WAYS; i++) {
		if (cacheline_ways[i].__line_valid == INVALID) { //空条目
			cacheline_ways[i].__line_valid = VALID;
			*selected_index = i; //成功之后再赋值
			cacheline_ways[i].__dirty = CLEAN; //空条目不用刷回，虽然初始值可能就是0
			cacheline_ways[i].__count = CACHELINE_WAYS_MAX; //调至最大，用来适应hit_fresh算法
			return SUCCESS;
		}
	}

	for (size_t i = 0; i < CACHELINE_WAYS; i++) {
		if (cacheline_ways[i].__count == CACHELINE_WAYS_MAX) { //自动适应hit_fresh算法
			*selected_index = i; //可能触发刷回，也可能不触发
			return SUCCESS;
		}
	}

	return FAILURE;
}



DS_Cache& g_ds_Cache = DS_Cache::get_ds_instance();


/*ds Cache API*/

status read_ds_cache(void* addr, void* buffer, size_t n_bytes) {
	return g_ds_Cache.read(addr, buffer, n_bytes);
}

status write_ds_cache(void* addr, void* buffer, size_t n_bytes, bool write_back) {
	return g_ds_Cache.write(addr, buffer, n_bytes, write_back);
}

status clear_ds_cache() {
	return g_ds_Cache.clear_cache();
}
