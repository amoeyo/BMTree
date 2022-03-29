#include "cache.h"

//加速恢复表
phy_addr_t* AccelerateBMTRecoverTable = 0;
//不做类型检查了

uint8_t* cacheline_register = nullptr; //64B共享大寄存器
uint8_t* cacheline_register_extra = nullptr;

/*inline tool func*/
static inline void clear_cacheline_register(cacheline_t cacheline_reg) {
	memset(cacheline_reg, 0, CACHELINE);
}

static inline void cache_read_statistics(bool cacheline_hit) {
	if (cacheline_hit) {
		g_Statistics.inc_cache_hit_cntr();
		g_Statistics.inc_cache_read_hit_cntr();
	}
	else {
		g_Statistics.inc_cache_miss_cntr();
		g_Statistics.inc_cache_read_miss_cntr();
	}
}

static inline void cache_write_statistics(bool cacheline_hit) {
	if (cacheline_hit) {
		g_Statistics.inc_cache_hit_cntr();
		g_Statistics.inc_cache_write_hit_cntr();
	}
	else {
		g_Statistics.inc_cache_miss_cntr();
		g_Statistics.inc_cache_write_miss_cntr();
	}
}

/*访存接口*/
//这个是数据搬运，cache调用进行操作,从cache往寄存器或者寄存器往cache，寄存器可能有64B的
status cache_data_move(void* dst, void* src, size_t n_bytes) {
	//check n_bytes <= sizeof(phy_addr_t)
	//if (n_bytes > ADDR_LEN) return PARAMETER_ERROR;
	memcpy(dst, src, n_bytes);
	//引入时延
	g_Statistics.t_lat_add(cache_latency_unit);
	g_Statistics.cache_lat_add(cache_latency_unit);
	return SUCCESS;
}

//模拟访存接口，cache调用进行操作且都是以cacheline为单元
//加密引擎都是通过cache来与内存交换数据，不会直接访存
status memory_data_read(void* addr, void* buffer, size_t n_bytes) {
	//assert n_bytes == CACHELINE
	//if (n_bytes != CACHELINE) return PARAMETER_ERROR;
	memcpy(buffer, addr, n_bytes);  //memmove()
	//引入时延
	g_Statistics.t_lat_add(memory_read_latency_unit);
	g_Statistics.memory_read_lat_add(memory_read_latency_unit);
	g_Statistics.inc_engine_extra_mem_bmt_read_cntr();
	return SUCCESS;
}

status memory_data_write(void* addr, void* buffer, size_t n_bytes) {
	//assert n_bytes == CACHELINE
	//if (n_bytes != CACHELINE) return PARAMETER_ERROR;
	memcpy(addr, buffer, n_bytes); //write_acc_table调用
	//引入时延
	g_Statistics.t_lat_add(memory_write_latency_unit);
	g_Statistics.memory_write_lat_add(memory_write_latency_unit);
	g_Statistics.inc_engine_extra_mem_bmt_write_cntr();
	return SUCCESS;
}

//class Cache definition

Cache::Cache() {
	AccelerateBMTRecoverTable = (phy_addr_t*)memory_alloc(CACHELINE_RANGE * sizeof(phy_addr_t), PAGE);
	memset(cache, 0, sizeof(cache));
	cacheline_register = (addr_t)memory_alloc(CACHELINE * sizeof(uint8_t), CACHELINE); //64B共享大寄存器
	cacheline_register_extra = (addr_t)memory_alloc(CACHELINE * sizeof(uint8_t), CACHELINE);
	memset(cacheline_register, 0, sizeof(uint8_t) * CACHELINE);
	memset(cacheline_register_extra, 0, sizeof(uint8_t) * CACHELINE);
}

status 
Cache::read(void* addr, void* buffer, size_t n_bytes,
	eviction_cb_t eviction_cb, bool use_acc_table) {
	if (!addr || !buffer || !n_bytes) return PARAMETER_ERROR; //中途的异常检查省略
	//if (n_bytes > ADDR_LEN) return PARAMETER_ERROR;
	g_Statistics.inc_cache_read_called_cntr();
	return __read((phy_addr_t)addr, (addr_t)buffer, n_bytes, eviction_cb, use_acc_table);
}

//默认不写回，不用acc table
status 
Cache::write(void* addr, void* buffer, size_t n_bytes,
	eviction_cb_t eviction_cb, bool write_back, bool use_acc_table) {
	if (!addr || !buffer || !n_bytes) return PARAMETER_ERROR; //中途的异常检查省略
	//if (n_bytes > ADDR_LEN) return PARAMETER_ERROR;
	g_Statistics.inc_cache_write_called_cntr();
	return __write((phy_addr_t)addr, (addr_t)buffer, n_bytes, eviction_cb, write_back, use_acc_table);
}

status 
Cache::clear_cache() {
	memset(cache, 0, sizeof(cache));
	return SUCCESS;
}

status 
Cache::__read(phy_addr_t addr, addr_t buffer, size_t n_bytes, eviction_cb_t eviction_cb, bool use_acc_table) {
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
			g_Statistics.inc_cache_read_eviction_cntr();
			g_Statistics.inc_cache_eviction_cntr();
			memory_data_write((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);//写回NVM
			if (eviction_cb) eviction_cb(__addr);
			cacheline_ways[lru_eviction_index].__dirty = CLEAN;

			//下面的乘法可以通过移位完成
			//baseline和strict persist不需要
			if (use_acc_table) {
				size_t acc_table_idx = ((size_t)group_index) * CACHELINE_WAYS + lru_eviction_index;
				write_acc_table(AccelerateBMTRecoverTable, acc_table_idx, 0);
			}
		}
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_read((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE); //从NVM读对应的counter节点
		cacheline_ways[lru_eviction_index].__tag = tag;
	}

	cache_data_move(buffer, cacheline_ways[lru_eviction_index].__data + bytes_index, __n_bytes);
	hit_fresh(cacheline_ways, lru_eviction_index); //调整计数器

	status other_hit = true;
	if (__n_bytes < n_bytes) { //越界了
		other_hit = __read(addr + __n_bytes, buffer + __n_bytes, n_bytes - __n_bytes, eviction_cb, use_acc_table);
	}

	return (cacheline_hit && other_hit) ? CACHE_HIT : CACHE_MISS;
}

status 
Cache::__write(phy_addr_t addr, addr_t buffer, size_t n_bytes,
	eviction_cb_t eviction_cb, bool write_back, bool use_acc_table) {
	// buffer -> addr
	phy_addr_t tag = __parse_tag(addr);
	phy_addr_t group_index = __parse_grp_idx(addr);
	size_t bytes_index = size_t(__parse_bytes_idx(addr));

	Cacheline_t* cacheline_ways = cache[group_index];
	size_t lru_eviction_index = 0;
	size_t __n_bytes = min_xy(CACHELINE - bytes_index, n_bytes);

	bool cacheline_hit = false;
	bool need_write_acc_table = false;
	for (size_t idx = 0; idx < CACHELINE_WAYS; idx++) {
		if (cacheline_ways[idx].__line_valid == VALID && cacheline_ways[idx].__tag == tag) {
			lru_eviction_index = idx; //之后调整所有比这个条目计数器小的计数器
			cacheline_hit = true; //命中直接继续处理
			need_write_acc_table = (cacheline_ways[idx].__dirty == CLEAN);
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
			if (eviction_cb) eviction_cb(__addr);
			g_Statistics.inc_cache_write_eviction_cntr();
			g_Statistics.inc_cache_eviction_cntr();
		}
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_read((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);//将页提取上来
		cacheline_ways[lru_eviction_index].__tag = tag;
		need_write_acc_table = true;
	}

	cache_data_move(cacheline_ways[lru_eviction_index].__data + bytes_index, buffer, __n_bytes);
	hit_fresh(cacheline_ways, lru_eviction_index); //调整计数器

	if (use_acc_table && need_write_acc_table) {
		//下面的乘法可以通过移位完成
		size_t acc_table_idx = ((size_t)group_index) * CACHELINE_WAYS + lru_eviction_index;
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		write_acc_table(AccelerateBMTRecoverTable, acc_table_idx, __addr); //记录cacheline地址就好，暂时只写但是不用
	}

	if (write_back) {
		if (use_acc_table) {
			size_t acc_table_idx = ((size_t)group_index) * CACHELINE_WAYS + lru_eviction_index;
			//phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
			write_acc_table(AccelerateBMTRecoverTable, acc_table_idx, 0);
		}
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_write((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);
	}
	cacheline_ways[lru_eviction_index].__dirty = (write_back ? CLEAN : DIRTY);

	status other_hit = true;
	if (__n_bytes < n_bytes) { //越界了
		other_hit = __write(addr + __n_bytes, buffer + __n_bytes, n_bytes - __n_bytes, eviction_cb, write_back, use_acc_table);
	}

	return (cacheline_hit && other_hit) ? CACHE_HIT : CACHE_MISS;
}

//刷新规则：
	/// @brief : 空条目或被驱逐条目的计数器会被调整至最大
	/// @brief : 比被选中的条目计数器小的有效计数器会自增，之后选中条目计数器置0

status 
Cache::hit_fresh(Cacheline_t* cacheline_ways, size_t hit_index) {
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
Cache::select_cacheline(Cacheline_t* cacheline_ways, size_t* selected_index) {
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

//地址跟踪，访存
status 
Cache::write_acc_table(phy_addr_t* acc_table, size_t idx, phy_addr_t addr) {
	g_Statistics.inc_accelerate_bmt_recover_table_cntr();
	return memory_data_write(acc_table + idx, &addr, sizeof(addr));
}

Cache& g_Cache = Cache::get_instance();


/*Cache API*/

status read_cache(void* addr, void* buffer, size_t n_bytes, 
					eviction_cb_t eviction_cb, bool use_acc_table) {
	return g_Cache.read(addr, buffer, n_bytes, eviction_cb, use_acc_table);
}

status write_cache(void* addr, void* buffer, size_t n_bytes, 
					eviction_cb_t eviction_cb, bool write_back, bool use_acc_table) {
	return g_Cache.write(addr, buffer, n_bytes, eviction_cb, write_back, use_acc_table);
}

status clear_cache() {
	return g_Cache.clear_cache();
}

