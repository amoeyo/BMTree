#include "regionIdxTable.h"

//Statistics& g_Statistics = Statistics::get_instance();


/*Tool func*/
/*RegisterTool*/
status 
__RegisterTool::get_value(table_size_t idx_K_bits, uint64_t mask, uint64_t* value) {
	table_size_t idx = idx_K_bits >> 3;
	table_size_t K = idx_K_bits & 0x7;
	if (idx >= BOARD_BYTES) { //�����߽�
		K += ((idx - BOARD_BYTES) << 3);
		idx = BOARD_BYTES;
	}
	uint64_t data = *(uint64_t*)(cacheline + idx);
	*value = (data >> K) & mask; //������value���Ƿ���cache������Ƿ��ʼĴ���
	return SUCCESS;
}

//��cacheline�ĵ�idx_K_bits��bit��ʼ��64bit���ݣ�����Ϊmask����ָʾ��64bit���ö೤bit����valueֵ��
//status set_value(table_size_t idx_K_bits, table_size_t mask, uint64_t value, size_t lat_factor = 1) {
status 
__RegisterTool::set_value(table_size_t idx_K_bits, uint64_t mask, uint64_t value, bool write_back) {
	table_size_t idx = idx_K_bits >> 3;
	table_size_t K = idx_K_bits & 0x7;
	if (idx >= BOARD_BYTES) { //�����߽�
		K += ((idx - BOARD_BYTES) << 3);
		idx = BOARD_BYTES;
	}
	value &= mask;
	value <<= K;
	uint64_t erase = MAKE_ZERO_ERASE(mask, K, uint64_t);

	uint64_t* data = (uint64_t*)(cacheline + idx); //����cache
	*data = (*data & erase) | value; //����cache
	return SUCCESS;
}

/*AlignedCacheline*/
//�ӳٿ�������������ӣ�����������Ὣcacheline�����ַ��Ӧ�����ݶ�ȡ����������������
//��K���ֽڵĿ�ʼ��unit_bits������value��Ӧ�ĵ�ַ��Ӧ���������ڲ��ڲ���ַ�������ڴ��ַ
status __AlignedCacheline::get_value(table_size_t idx_K_bits, uint64_t mask, uint64_t* value) {
	table_size_t idx = idx_K_bits >> 3;
	table_size_t K = idx_K_bits & 0x7;
	if (idx >= BOARD_BYTES) { //�����߽�
		K += ((idx - BOARD_BYTES) << 3);
		idx = BOARD_BYTES;
	}
	uint64_t data = 0;

	//������ֱ�Ӵ�Ĵ�����ģʽ������������û��̫���Ҫ����ҪBMT����Ctr,BMT��Ctr����������
	//data = *((uint64_t*)(cacheline + idx));//��Ĵ��������__RegisterTool

	//������100%cache���У����ö���Ļ������򣬵�ȻҲ���Է���һ����
#ifdef USING_EXTRA_CACHE_FOR_INDEX_TABLE

	/*	g_Statistics.inc_cache_hit_cntr();
		g_Statistics.inc_cache_read_hit_cntr();
		g_Statistics.inc_cache_read_called_cntr();
		cache_data_move(&data, cacheline + idx, sizeof(data));*/

	status stat = read_dr_cache(cacheline + idx, &data, sizeof(data));

	if (stat != CACHE_HIT) {
		// δ����Ҫ��ȥд���ӳ�
		latency_t lat = g_Statistics.get_t_latency();
		lat -= memory_write_latency_unit;
		g_Statistics.set_t_latency(lat);
	}

#else
		//�����ǹ���һ��cache�ķ�ʽ
	read_cache(cacheline + idx, &data, sizeof(data));
#endif // USING_EXTRA_CACHE_FOR_INDEX_TABLE

	* value = (data >> K) & mask; //������value���Ƿ���cache������Ƿ��ʼĴ���
	return SUCCESS;
}

//��cacheline�ĵ�idx_K_bits��bit��ʼ��64bit���ݣ�����Ϊmask����ָʾ��64bit���ö೤bit����valueֵ��
//status set_value(table_size_t idx_K_bits, table_size_t mask, uint64_t value, size_t lat_factor = 1) {
status __AlignedCacheline::set_value(table_size_t idx_K_bits, uint64_t mask, uint64_t value, bool write_back) {
	table_size_t idx = idx_K_bits >> 3;
	table_size_t K = idx_K_bits & 0x7;
	if (idx >= BOARD_BYTES) { //�����߽�
		K += ((idx - BOARD_BYTES) << 3);
		idx = BOARD_BYTES;
	}
	value &= mask;
	value <<= K;
	uint64_t erase = MAKE_ZERO_ERASE(mask, K, uint64_t);

	//�����Ǵ�Ĵ���ģʽ
	//uint64_t* data = (uint64_t*)(cacheline + idx); //����cache
	//*data = (*data & erase) | value; //����cache

	//������100%����ģʽ�����⻺������
#ifdef USING_EXTRA_CACHE_FOR_INDEX_TABLE
		//uint64_t data = 0;//�Ĵ���
		//g_Statistics.inc_cache_hit_cntr();
		//g_Statistics.inc_cache_read_hit_cntr();
		//g_Statistics.inc_cache_read_called_cntr();
		//cache_data_move(&data, cacheline + idx, sizeof(data));

		//data = (data & erase) | value;
		//g_Statistics.inc_cache_hit_cntr();
		//g_Statistics.inc_cache_write_hit_cntr();
		//g_Statistics.inc_cache_write_called_cntr();
		//cache_data_move(cacheline + idx, &data, sizeof(data));
		////���ﻹӦ�ü�һ���ڴ�д�ӳ�
		//if (write_back) {
		//	g_Statistics.inc_engine_extra_mem_index_write_cntr();
		//	memory_data_write(cacheline + idx, &data, sizeof(data));
		//}
	uint64_t data = 0;
	read_dr_cache(cacheline + idx, &data, sizeof(data));
	data = (data & erase) | value;
	write_dr_cache(cacheline + idx, &data, sizeof(data), write_back); //�ⲿ��ǿ��д��,�����ܴ�������Ĵ���
	g_Statistics.inc_engine_extra_mem_index_write_cntr();
#else
		//�����ǹ��û�����ģʽ
	uint64_t data = 0;
	read_cache(cacheline + idx, &data, sizeof(data));
	data = (data & erase) | value;
	write_cache(cacheline + idx, &data, sizeof(data), nullptr, write_back); //�ⲿ��ǿ��д��,�����ܴ�������Ĵ���
	g_Statistics.inc_engine_extra_mem_index_write_cntr();
#endif // USING_EXTRA_CACHE_FOR_INDEX_TABLE
	return SUCCESS;
}

/*AlignedRegionIdxTable*/

AlignedRegionIdxTable::AlignedRegionIdxTable(table_size_t idx_range, table_size_t unit_bits)
	: __idx_range(idx_range), __unit_bits(unit_bits) {
	__cacheline_range = (CACHELINE << 3) / __unit_bits; //�������ֻ����һ��
	__base = memory_alloc(((size_t)CEIL_DIV(__idx_range, __cacheline_range)) << CACHELINE_BITS, PAGE);
	__mask = MAKE_MASK(unit_bits, uint64_t);
}

//�ᷢ��������������ڷ���__cacheilne_range��Щ˽�б����ᴥ�����ʻ��濪��
status 
AlignedRegionIdxTable::get_value(table_size_t idx_K, uint64_t* value) {
	//check idx_K < __idx_range
	table_size_t idx = idx_K / __cacheline_range; //���������__cacheline_range����Ļ�����������λ
	idx_K %= __cacheline_range;  //����ĳ˷�Ҳ��
	return (((__AlignedCacheline*)__base) + idx)->get_value(idx_K * __unit_bits, __mask, value);
}

//�ṩ����cacheline����
status 
AlignedRegionIdxTable::get_cachline(table_size_t cacheline_idx, cacheline_t buffer, uint32_t n_bytes) {
	addr_t __addr = __base + (cacheline_idx << CACHELINE_BITS);
#ifdef USING_EXTRA_CACHE_FOR_INDEX_TABLE
	//g_Statistics.inc_cache_hit_cntr();
	//g_Statistics.inc_cache_read_hit_cntr();
	//g_Statistics.inc_cache_read_called_cntr();
	//return cache_data_move(buffer, __addr, n_bytes); //100% cache���е�
	status stat = read_dr_cache(__addr, buffer, sizeof(buffer));
	if (stat != CACHE_HIT) {
		// δ����Ҫ��ȥд���ӳ�
		latency_t lat = g_Statistics.get_t_latency();
		lat -= memory_write_latency_unit;
		g_Statistics.set_t_latency(lat);
	}
	return stat;

#else
	return read_cache(__addr, buffer, n_bytes); //�����ǹ���cache��
#endif // USING_EXTRA_CACHE_FOR_INDEX_TABLE

}

//����д���ӿڣ�����fore_index����
status 
AlignedRegionIdxTable::write_cacheline(table_size_t cacheline_idx, cacheline_t buffer, uint32_t n_bytes) {
	addr_t __addr = __base + (cacheline_idx << CACHELINE_BITS);
#ifdef USING_EXTRA_CACHE_FOR_INDEX_TABLE
	/*g_Statistics.inc_cache_hit_cntr();
	g_Statistics.inc_cache_write_hit_cntr();
	g_Statistics.inc_cache_write_called_cntr();
	return cache_data_move(__addr, buffer, n_bytes);*/
	g_Statistics.inc_engine_extra_mem_index_write_cntr();
	return write_dr_cache(__addr, buffer, sizeof(buffer), true); //�ⲿ��ǿ��д��,�����ܴ�������Ĵ���

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
	return cache_data_move(buffer, __addr, n_bytes); //100%����
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