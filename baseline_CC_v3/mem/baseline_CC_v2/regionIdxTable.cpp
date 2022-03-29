#include "regionIdxTable.h"

//Statistics& g_Statistics = Statistics::get_instance();


/*Tool func*/
/*RegisterTool*/
status 
__RegisterTool::get_value(table_size_t idx_K_bits, uint64_t mask, uint64_t* value) {
	table_size_t idx = idx_K_bits >> 3;
	table_size_t K = idx_K_bits & 0x7;
	if (idx >= BOARD_BYTES) { //修正边界
		K += ((idx - BOARD_BYTES) << 3);
		idx = BOARD_BYTES;
	}
	uint64_t data = *(uint64_t*)(cacheline + idx);
	*value = (data >> K) & mask; //像这种value不是访问cache，这个是访问寄存器
	return SUCCESS;
}

//将cacheline的第idx_K_bits个bit开始的64bit数据，设置为mask掩码指示（64bit设置多长bit）的value值。
//status set_value(table_size_t idx_K_bits, table_size_t mask, uint64_t value, size_t lat_factor = 1) {
status 
__RegisterTool::set_value(table_size_t idx_K_bits, uint64_t mask, uint64_t value, bool write_back) {
	table_size_t idx = idx_K_bits >> 3;
	table_size_t K = idx_K_bits & 0x7;
	if (idx >= BOARD_BYTES) { //修正边界
		K += ((idx - BOARD_BYTES) << 3);
		idx = BOARD_BYTES;
	}
	value &= mask;
	value <<= K;
	uint64_t erase = MAKE_ZERO_ERASE(mask, K, uint64_t);

	uint64_t* data = (uint64_t*)(cacheline + idx); //访问cache
	*data = (*data & erase) | value; //访问cache
	return SUCCESS;
}

/*AlignedCacheline*/
//延迟可以在这个函数加，这个函数，会将cacheline这个地址对应的数据都取出来，再做操作。
//第K个字节的开始的unit_bits，这种value对应的地址，应该是引擎内部内部地址，而非内存地址
status __AlignedCacheline::get_value(table_size_t idx_K_bits, uint64_t mask, uint64_t* value) {
	table_size_t idx = idx_K_bits >> 3;
	table_size_t K = idx_K_bits & 0x7;
	if (idx >= BOARD_BYTES) { //修正边界
		K += ((idx - BOARD_BYTES) << 3);
		idx = BOARD_BYTES;
	}
	uint64_t data = 0;

	//以下是直接大寄存器的模式，但是索引表没有太大必要，不要BMT树的Ctr,BMT的Ctr单独处理了
	//data = *((uint64_t*)(cacheline + idx));//大寄存器提出来__RegisterTool

	//以下是100%cache命中，采用额外的缓存区域，当然也可以放在一起用
#ifdef USING_EXTRA_CACHE_FOR_INDEX_TABLE

	/*	g_Statistics.inc_cache_hit_cntr();
		g_Statistics.inc_cache_read_hit_cntr();
		g_Statistics.inc_cache_read_called_cntr();
		cache_data_move(&data, cacheline + idx, sizeof(data));*/

	status stat = read_dr_cache(cacheline + idx, &data, sizeof(data));

	if (stat != CACHE_HIT) {
		// 未命中要减去写回延迟
		latency_t lat = g_Statistics.get_t_latency();
		lat -= memory_write_latency_unit;
		g_Statistics.set_t_latency(lat);
	}

#else
		//以下是共用一个cache的方式
	read_cache(cacheline + idx, &data, sizeof(data));
#endif // USING_EXTRA_CACHE_FOR_INDEX_TABLE

	* value = (data >> K) & mask; //像这种value不是访问cache，这个是访问寄存器
	return SUCCESS;
}

//将cacheline的第idx_K_bits个bit开始的64bit数据，设置为mask掩码指示（64bit设置多长bit）的value值。
//status set_value(table_size_t idx_K_bits, table_size_t mask, uint64_t value, size_t lat_factor = 1) {
status __AlignedCacheline::set_value(table_size_t idx_K_bits, uint64_t mask, uint64_t value, bool write_back) {
	table_size_t idx = idx_K_bits >> 3;
	table_size_t K = idx_K_bits & 0x7;
	if (idx >= BOARD_BYTES) { //修正边界
		K += ((idx - BOARD_BYTES) << 3);
		idx = BOARD_BYTES;
	}
	value &= mask;
	value <<= K;
	uint64_t erase = MAKE_ZERO_ERASE(mask, K, uint64_t);

	//以下是大寄存器模式
	//uint64_t* data = (uint64_t*)(cacheline + idx); //访问cache
	//*data = (*data & erase) | value; //访问cache

	//以下是100%命中模式，额外缓存区域
#ifdef USING_EXTRA_CACHE_FOR_INDEX_TABLE
		//uint64_t data = 0;//寄存器
		//g_Statistics.inc_cache_hit_cntr();
		//g_Statistics.inc_cache_read_hit_cntr();
		//g_Statistics.inc_cache_read_called_cntr();
		//cache_data_move(&data, cacheline + idx, sizeof(data));

		//data = (data & erase) | value;
		//g_Statistics.inc_cache_hit_cntr();
		//g_Statistics.inc_cache_write_hit_cntr();
		//g_Statistics.inc_cache_write_called_cntr();
		//cache_data_move(cacheline + idx, &data, sizeof(data));
		////这里还应该加一个内存写延迟
		//if (write_back) {
		//	g_Statistics.inc_engine_extra_mem_index_write_cntr();
		//	memory_data_write(cacheline + idx, &data, sizeof(data));
		//}
	uint64_t data = 0;
	read_dr_cache(cacheline + idx, &data, sizeof(data));
	data = (data & erase) | value;
	write_dr_cache(cacheline + idx, &data, sizeof(data), write_back); //这部分强制写穿,还可能带来驱逐的次数
	g_Statistics.inc_engine_extra_mem_index_write_cntr();
#else
		//以下是共用缓存区模式
	uint64_t data = 0;
	read_cache(cacheline + idx, &data, sizeof(data));
	data = (data & erase) | value;
	write_cache(cacheline + idx, &data, sizeof(data), nullptr, write_back); //这部分强制写穿,还可能带来驱逐的次数
	g_Statistics.inc_engine_extra_mem_index_write_cntr();
#endif // USING_EXTRA_CACHE_FOR_INDEX_TABLE
	return SUCCESS;
}

/*AlignedRegionIdxTable*/

AlignedRegionIdxTable::AlignedRegionIdxTable(table_size_t idx_range, table_size_t unit_bits)
	: __idx_range(idx_range), __unit_bits(unit_bits) {
	__cacheline_range = (CACHELINE << 3) / __unit_bits; //这个除法只触发一次
	__base = memory_alloc(((size_t)CEIL_DIV(__idx_range, __cacheline_range)) << CACHELINE_BITS, PAGE);
	__mask = MAKE_MASK(unit_bits, uint64_t);
}

//会发现这个函数，仅在访问__cacheilne_range这些私有变量会触发访问缓存开销
status 
AlignedRegionIdxTable::get_value(table_size_t idx_K, uint64_t* value) {
	//check idx_K < __idx_range
	table_size_t idx = idx_K / __cacheline_range; //除法，如果__cacheline_range对齐的话，可以是移位
	idx_K %= __cacheline_range;  //下面的乘法也是
	return (((__AlignedCacheline*)__base) + idx)->get_value(idx_K * __unit_bits, __mask, value);
}

//提供两个cacheline操作
status 
AlignedRegionIdxTable::get_cachline(table_size_t cacheline_idx, cacheline_t buffer, uint32_t n_bytes) {
	addr_t __addr = __base + (cacheline_idx << CACHELINE_BITS);
#ifdef USING_EXTRA_CACHE_FOR_INDEX_TABLE
	//g_Statistics.inc_cache_hit_cntr();
	//g_Statistics.inc_cache_read_hit_cntr();
	//g_Statistics.inc_cache_read_called_cntr();
	//return cache_data_move(buffer, __addr, n_bytes); //100% cache命中的
	status stat = read_dr_cache(__addr, buffer, sizeof(buffer));
	if (stat != CACHE_HIT) {
		// 未命中要减去写回延迟
		latency_t lat = g_Statistics.get_t_latency();
		lat -= memory_write_latency_unit;
		g_Statistics.set_t_latency(lat);
	}
	return stat;

#else
	return read_cache(__addr, buffer, n_bytes); //这种是共用cache的
#endif // USING_EXTRA_CACHE_FOR_INDEX_TABLE

}

//不带写穿接口，都是fore_index调用
status 
AlignedRegionIdxTable::write_cacheline(table_size_t cacheline_idx, cacheline_t buffer, uint32_t n_bytes) {
	addr_t __addr = __base + (cacheline_idx << CACHELINE_BITS);
#ifdef USING_EXTRA_CACHE_FOR_INDEX_TABLE
	/*g_Statistics.inc_cache_hit_cntr();
	g_Statistics.inc_cache_write_hit_cntr();
	g_Statistics.inc_cache_write_called_cntr();
	return cache_data_move(__addr, buffer, n_bytes);*/
	g_Statistics.inc_engine_extra_mem_index_write_cntr();
	return write_dr_cache(__addr, buffer, sizeof(buffer), true); //这部分强制写穿,还可能带来驱逐的次数

#else
	return write_cache(__addr, buffer, n_bytes);
#endif // USING_EXTRA_CACHE_FOR_INDEX_TABLE
}

status 
AlignedRegionIdxTable::get_bytes(table_size_t bytes_idx, addr_t buffer, uint32_t n_bytes) {
	addr_t __addr = __base + bytes_idx;
#ifdef USING_EXTRA_CACHE_FOR_INDEX_TABLE
	g_Statistics.inc_cache_hit_cntr();
	g_Statistics.inc_cache_read_hit_cntr();
	g_Statistics.inc_cache_read_called_cntr();
	return cache_data_move(buffer, __addr, n_bytes); //100%命中
#else
	return read_cache(__addr, buffer, n_bytes);
#endif // USING_EXTRA_CACHE_FOR_INDEX_TABLE
}

status 
AlignedRegionIdxTable::set_value(table_size_t idx_K, uint64_t value, bool write_back) {
	//check idx_K < __idx_range
	table_size_t idx = idx_K / __cacheline_range;
	idx_K %= __cacheline_range;
	return (((__AlignedCacheline*)__base) + idx)->set_value(idx_K * __unit_bits, __mask, value, write_back);
}

AlignedRegionIdxTable::~AlignedRegionIdxTable() { memory_free(__base); }