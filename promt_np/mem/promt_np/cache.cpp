#include "cache.h"

//���ٻָ���
phy_addr_t* AccelerateBMTRecoverTable = 0;
//�������ͼ����

uint8_t* cacheline_register = nullptr; //64B�����Ĵ���
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

/*�ô�ӿ�*/
//��������ݰ��ˣ�cache���ý��в���,��cache���Ĵ������߼Ĵ�����cache���Ĵ���������64B��
status cache_data_move(void* dst, void* src, size_t n_bytes) {
	//check n_bytes <= sizeof(phy_addr_t)
	//if (n_bytes > ADDR_LEN) return PARAMETER_ERROR;
	memcpy(dst, src, n_bytes);
	//����ʱ��
	g_Statistics.t_lat_add(cache_latency_unit);
	g_Statistics.cache_lat_add(cache_latency_unit);
	return SUCCESS;
}

//ģ��ô�ӿڣ�cache���ý��в����Ҷ�����cachelineΪ��Ԫ
//�������涼��ͨ��cache�����ڴ潻�����ݣ�����ֱ�ӷô�
status memory_data_read(void* addr, void* buffer, size_t n_bytes) {
	//assert n_bytes == CACHELINE
	//if (n_bytes != CACHELINE) return PARAMETER_ERROR;
	memcpy(buffer, addr, n_bytes);  //memmove()
	//����ʱ��
	g_Statistics.t_lat_add(memory_read_latency_unit);
	g_Statistics.memory_read_lat_add(memory_read_latency_unit);
	g_Statistics.inc_engine_extra_mem_bmt_read_cntr();
	return SUCCESS;
}

status memory_data_write(void* addr, void* buffer, size_t n_bytes) {
	//assert n_bytes == CACHELINE
	//if (n_bytes != CACHELINE) return PARAMETER_ERROR;
	memcpy(addr, buffer, n_bytes); //write_acc_table����
	//����ʱ��
	g_Statistics.t_lat_add(memory_write_latency_unit);
	g_Statistics.memory_write_lat_add(memory_write_latency_unit);
	g_Statistics.inc_engine_extra_mem_bmt_write_cntr();
	return SUCCESS;
}

//class Cache definition

Cache::Cache() {
	AccelerateBMTRecoverTable = (phy_addr_t*)memory_alloc(CACHELINE_RANGE * sizeof(phy_addr_t), PAGE);
	memset(cache, 0, sizeof(cache));
	cacheline_register = (addr_t)memory_alloc(CACHELINE * sizeof(uint8_t), CACHELINE); //64B�����Ĵ���
	cacheline_register_extra = (addr_t)memory_alloc(CACHELINE * sizeof(uint8_t), CACHELINE);
	memset(cacheline_register, 0, sizeof(uint8_t) * CACHELINE);
	memset(cacheline_register_extra, 0, sizeof(uint8_t) * CACHELINE);
}

status 
Cache::read(void* addr, void* buffer, size_t n_bytes,
	eviction_cb_t eviction_cb, bool use_acc_table) {
	if (!addr || !buffer || !n_bytes) return PARAMETER_ERROR; //��;���쳣���ʡ��
	//if (n_bytes > ADDR_LEN) return PARAMETER_ERROR;
	g_Statistics.inc_cache_read_called_cntr();
	return __read((phy_addr_t)addr, (addr_t)buffer, n_bytes, eviction_cb, use_acc_table);
}

//Ĭ�ϲ�д�أ�����acc table
status 
Cache::write(void* addr, void* buffer, size_t n_bytes,
	eviction_cb_t eviction_cb, bool write_back, bool use_acc_table) {
	if (!addr || !buffer || !n_bytes) return PARAMETER_ERROR; //��;���쳣���ʡ��
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
	size_t __n_bytes = min_xy(CACHELINE - bytes_index, n_bytes); //���ܿ���

	bool cacheline_hit = false;

	for (size_t idx = 0; idx < CACHELINE_WAYS; idx++) {
		if (cacheline_ways[idx].__line_valid == VALID && cacheline_ways[idx].__tag == tag) {
			lru_eviction_index = idx; //֮��������б������Ŀ������С�ļ�����
			cacheline_hit = true; //����ֱ�Ӽ�������
			break;
		}
	}
	cache_read_statistics(cacheline_hit);
	//û�����У���ѡһ����λ�������ݵ��������ٴ���
	if (!cacheline_hit) {
		select_cacheline(cacheline_ways, &lru_eviction_index); //ѡ�е���Ŀ�������ᱻ���������������������Ŀ������
		if (cacheline_ways[lru_eviction_index].__dirty == DIRTY) {//�ô�ˢ��
			phy_addr_t __tag = cacheline_ways[lru_eviction_index].__tag;
			phy_addr_t __addr = __comb_cacheline_addr(__tag, group_index);
			g_Statistics.inc_cache_read_eviction_cntr();
			g_Statistics.inc_cache_eviction_cntr();
			memory_data_write((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);//д��NVM
			if (eviction_cb) eviction_cb(__addr);
			cacheline_ways[lru_eviction_index].__dirty = CLEAN;

			//����ĳ˷�����ͨ����λ���
			//baseline��strict persist����Ҫ
			if (use_acc_table) {
				size_t acc_table_idx = ((size_t)group_index) * CACHELINE_WAYS + lru_eviction_index;
				write_acc_table(AccelerateBMTRecoverTable, acc_table_idx, 0);
			}
		}
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_read((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE); //��NVM����Ӧ��counter�ڵ�
		cacheline_ways[lru_eviction_index].__tag = tag;
	}

	cache_data_move(buffer, cacheline_ways[lru_eviction_index].__data + bytes_index, __n_bytes);
	hit_fresh(cacheline_ways, lru_eviction_index); //����������

	status other_hit = true;
	if (__n_bytes < n_bytes) { //Խ����
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
			lru_eviction_index = idx; //֮��������б������Ŀ������С�ļ�����
			cacheline_hit = true; //����ֱ�Ӽ�������
			need_write_acc_table = (cacheline_ways[idx].__dirty == CLEAN);
			break;
		}
	}
	cache_write_statistics(cacheline_hit);
	//��һ�α����µ�ʱ����Ҫ��¼һЩ����
	if (!cacheline_hit) {
		select_cacheline(cacheline_ways, &lru_eviction_index);
		if (cacheline_ways[lru_eviction_index].__dirty == DIRTY) {//�ô�ˢ��
			phy_addr_t __tag = cacheline_ways[lru_eviction_index].__tag;
			phy_addr_t __addr = __comb_cacheline_addr(__tag, group_index);
			memory_data_write((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);
			if (eviction_cb) eviction_cb(__addr);
			g_Statistics.inc_cache_write_eviction_cntr();
			g_Statistics.inc_cache_eviction_cntr();
		}
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_read((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);//��ҳ��ȡ����
		cacheline_ways[lru_eviction_index].__tag = tag;
		need_write_acc_table = true;
	}

	cache_data_move(cacheline_ways[lru_eviction_index].__data + bytes_index, buffer, __n_bytes);
	hit_fresh(cacheline_ways, lru_eviction_index); //����������

	if (use_acc_table && need_write_acc_table) {
		//����ĳ˷�����ͨ����λ���
		size_t acc_table_idx = ((size_t)group_index) * CACHELINE_WAYS + lru_eviction_index;
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		write_acc_table(AccelerateBMTRecoverTable, acc_table_idx, __addr); //��¼cacheline��ַ�ͺã���ʱֻд���ǲ���
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
	if (__n_bytes < n_bytes) { //Խ����
		other_hit = __write(addr + __n_bytes, buffer + __n_bytes, n_bytes - __n_bytes, eviction_cb, write_back, use_acc_table);
	}

	return (cacheline_hit && other_hit) ? CACHE_HIT : CACHE_MISS;
}

//ˢ�¹���
	/// @brief : ����Ŀ��������Ŀ�ļ������ᱻ���������
	/// @brief : �ȱ�ѡ�е���Ŀ������С����Ч��������������֮��ѡ����Ŀ��������0

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

//selected_index����һ���Ĵ�����ַ�����Ƿô�
status 
Cache::select_cacheline(Cacheline_t* cacheline_ways, size_t* selected_index) {
	for (size_t i = 0; i < CACHELINE_WAYS; i++) {
		if (cacheline_ways[i].__line_valid == INVALID) { //����Ŀ
			cacheline_ways[i].__line_valid = VALID;
			*selected_index = i; //�ɹ�֮���ٸ�ֵ
			cacheline_ways[i].__dirty = CLEAN; //����Ŀ����ˢ�أ���Ȼ��ʼֵ���ܾ���0
			cacheline_ways[i].__count = CACHELINE_WAYS_MAX; //�������������Ӧhit_fresh�㷨
			return SUCCESS;
		}
	}

	for (size_t i = 0; i < CACHELINE_WAYS; i++) {
		if (cacheline_ways[i].__count == CACHELINE_WAYS_MAX) { //�Զ���Ӧhit_fresh�㷨
			*selected_index = i; //���ܴ���ˢ�أ�Ҳ���ܲ�����
			return SUCCESS;
		}
	}

	return FAILURE;
}

//��ַ���٣��ô�
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

