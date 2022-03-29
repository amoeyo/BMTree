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
	if (!addr || !buffer || !n_bytes) return PARAMETER_ERROR; //��;���쳣���ʡ��
	//if (n_bytes > ADDR_LEN) return PARAMETER_ERROR;
	return __read((phy_addr_t)addr, (addr_t)buffer, n_bytes);
}

//Ĭ�ϲ�д�أ�����acc table
status
DS_Cache::write(void* addr, void* buffer, size_t n_bytes, bool write_back) {
	if (!addr || !buffer || !n_bytes) return PARAMETER_ERROR; //��;���쳣���ʡ��
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
			memory_data_write((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);//д��NVM
			cacheline_ways[lru_eviction_index].__dirty = CLEAN;
		}
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_read((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE); //��NVM����Ӧ��counter�ڵ�
		cacheline_ways[lru_eviction_index].__tag = tag;
	}

	cache_data_move(buffer, cacheline_ways[lru_eviction_index].__data + bytes_index, __n_bytes);
	hit_fresh(cacheline_ways, lru_eviction_index); //����������

	status other_hit = true;
	if (__n_bytes < n_bytes) { //Խ����
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
			lru_eviction_index = idx; //֮��������б������Ŀ������С�ļ�����
			cacheline_hit = true; //����ֱ�Ӽ�������
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
		}
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_read((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);//��ҳ��ȡ����
		cacheline_ways[lru_eviction_index].__tag = tag;
	}

	cache_data_move(cacheline_ways[lru_eviction_index].__data + bytes_index, buffer, __n_bytes);
	hit_fresh(cacheline_ways, lru_eviction_index); //����������

	if (write_back) {
		phy_addr_t __addr = __comb_cacheline_addr(tag, group_index);
		memory_data_write((addr_t)__addr, cacheline_ways[lru_eviction_index].__data, CACHELINE);
	}
	cacheline_ways[lru_eviction_index].__dirty = (write_back ? CLEAN : DIRTY);

	status other_hit = true;
	if (__n_bytes < n_bytes) { //Խ����
		other_hit = __write(addr + __n_bytes, buffer + __n_bytes, n_bytes - __n_bytes, write_back);
	}

	return (cacheline_hit && other_hit) ? CACHE_HIT : CACHE_MISS;
}

//ˢ�¹���
	/// @brief : ����Ŀ��������Ŀ�ļ������ᱻ���������
	/// @brief : �ȱ�ѡ�е���Ŀ������С����Ч��������������֮��ѡ����Ŀ��������0

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

//selected_index����һ���Ĵ�����ַ�����Ƿô�
status
DS_Cache::select_cacheline(Cacheline_t* cacheline_ways, size_t* selected_index) {
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
