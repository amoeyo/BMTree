#include "unionCtr.h"

status reg_data_move(void* dst, void* src, size_t n_bytes) {
	//�������
	//�Ĵ������ݰᶯ����ʱ������ʱ��
	memcpy(dst, src, n_bytes);
	return SUCCESS;
}

status 
RegisterUnionCtr::getValue(uint32_t ctr_K, major_t* major, uint64_t* minor) {
	//check ctr_K < 64
	__get_major(major);
	__get_K_minor(ctr_K, minor); //�ӵ��ֽڿ�ʼ���
	return SUCCESS;
}

void RegisterUnionCtr::__set_major(major_t major) { __major = major; }
void RegisterUnionCtr::__get_major(major_t* major) { *major = __major; }

status RegisterUnionCtr::__set_K_minor(table_size_t idx_K, uint64_t value) {
	return ((__RegisterTool*)this)->set_value(idx_K * 7, 0x7f, value);
}

status RegisterUnionCtr::__get_K_minor(table_size_t idx_K, uint64_t* value) {
	return ((__RegisterTool*)this)->get_value(idx_K * 7, 0x7f, value);
}

status RegisterUnionCtr::set_K_minor(table_size_t idx_K, uint64_t minor) {
	if (minor == 0x80) {
		__set_K_minor(idx_K, 0);
#ifdef IGNORE_MINOR_OVERFLOW
		return SUCCESS;
#else
		__set_major(__major + 1); //ֱ�Ӳ�����Ĵ������������ʱ��
		return MINOR_OVERFLOW;//ͬ��UnionCtr������OTP��Ҫ�������ɣ��������¼���
#endif // IGNORE_MINOR_OVERFLOW
	}
	else {
		__set_K_minor(idx_K, minor);
	}
	return SUCCESS;
}

status MemoryUnionCtr::getValue(uint32_t ctr_K, major_t* major, uint64_t* minor) {
	//check ctr_K < 64
	//check buffer != 0
	__get_major(major);
	__get_K_minor(ctr_K, minor); //�ӵ��ֽڿ�ʼ���
	return SUCCESS;
}

status MemoryUnionCtr::__set_major(uint64_t major) {
	return ((__AlignedCacheline*)this)->set_value(MINOR_RANGE << 3, ~0, major);
}
status MemoryUnionCtr::__get_major(uint64_t* major) {
	return ((__AlignedCacheline*)this)->get_value(MINOR_RANGE << 3, ~0, major);
}

status MemoryUnionCtr::__increase_major() {
	uint64_t major = 0;
	__get_major(&major);
	++major;
	__set_major(major);
	return SUCCESS;
}

status MemoryUnionCtr::__set_K_minor(table_size_t idx_K, uint64_t value) {
	return ((__AlignedCacheline*)this)->set_value(idx_K * 7, 0x7f, value);
}

status MemoryUnionCtr::__get_K_minor(table_size_t idx_K, uint64_t* value) {
	return ((__AlignedCacheline*)this)->get_value(idx_K * 7, 0x7f, value);
}

status MemoryUnionCtr::set_K_minor(table_size_t idx_K, uint64_t minor) {
	if (minor == 0x80) {
		__set_K_minor(idx_K, 0);
		__increase_major(); //ֱ�Ӳ�����Ĵ������������ʱ��
		return MINOR_OVERFLOW;//ͬ��UnionCtr������OTP��Ҫ�������ɣ��������¼���
	}
	else {
		__set_K_minor(idx_K, minor);
	}
	return SUCCESS;
}

status CPTA_t::read(void* addr, void* buffer, size_t n_bytes, eviction_cb_t eviction_cb) {
	if (!addr || !buffer || !n_bytes) return PARAMETER_ERROR; //��;���쳣���ʡ��
	return __read((phy_addr_t)addr, (addr_t)buffer, n_bytes, eviction_cb);
}

//�������Ҫ��һ���ص�����
status CPTA_t::write(void* addr, void* buffer, size_t n_bytes,
	eviction_cb_t eviction_cb, bool write_back) {
	if (!addr || !buffer || !n_bytes) return PARAMETER_ERROR; //��;���쳣���ʡ��
	write_cache(addr, buffer, n_bytes, nullptr, write_back); //���������д��cache��
	return __write((phy_addr_t)addr, (addr_t)buffer, n_bytes, eviction_cb);
}

status CPTA_t::hit_fresh(size_t hit_index) {
	phy_addr_t __count = pst_reg_array[hit_index].__count;
	for (size_t idx = 0; idx < N_CTR_PERSISTENT_REGISTER; idx++) {
		if (pst_reg_array[hit_index].__line_valid == VALID
			&& pst_reg_array[hit_index].__count < __count) {
			++pst_reg_array[hit_index].__count;
		}
	}
	pst_reg_array[hit_index].__count = 0;
	return SUCCESS;
}

status CPTA_t::select_pst_reg(size_t* selected_index) {
	for (size_t idx = 0; idx < N_CTR_PERSISTENT_REGISTER; idx++) {
		if (pst_reg_array[idx].__line_valid == INVALID) {
			pst_reg_array[idx].__line_valid = VALID;
			*selected_index = idx;
			pst_reg_array[idx].__dirty = CLEAN;
			pst_reg_array[idx].__count = CTR_PERSISTENT_REGISTER_MAX;
			return SUCCESS;
		}
	}

	for (size_t idx = 0; idx < N_CTR_PERSISTENT_REGISTER; idx++) {
		if (pst_reg_array[idx].__count == CTR_PERSISTENT_REGISTER_MAX) {
			*selected_index = idx;
			return SUCCESS;
		}
	}

	return FAILURE;
}
//�����Ӧ���ǲ�����ʲô�ӳٵ�
status CPTA_t::__read(phy_addr_t addr, addr_t buffer, size_t n_bytes,
	eviction_cb_t eviction_cb) {
	//check addr cacheline align
	phy_addr_t tag = __parse_pst_reg_tag(addr);
	size_t lru_eviction_index = 0;
	bool pst_reg_hit = false;

	for (size_t idx = 0; idx < N_CTR_PERSISTENT_REGISTER; idx++) {
		if (pst_reg_array[idx].__line_valid == VALID
			&& pst_reg_array[idx].__tag == tag) { //�������tag��ͬ��������
			pst_reg_hit = true;
			reg_data_move(buffer, pst_reg_array[idx].__data, CACHELINE);
			hit_fresh(idx);
			return CACHE_HIT;
		}
	}

	if (!pst_reg_hit) {
		select_pst_reg(&lru_eviction_index);
		if (pst_reg_array[lru_eviction_index].__dirty == DIRTY) { //����victim node�滻
			phy_addr_t __addr = __combine_pst_reg_addr(pst_reg_array[lru_eviction_index].__tag);
			// д�ػ���
			write_cache((addr_t)__addr, pst_reg_array[lru_eviction_index].__data, CACHELINE);
			if (eviction_cb) eviction_cb(__addr);
			pst_reg_array[lru_eviction_index].__dirty = CLEAN;
		}
		// �Ȱ��������ڲ�����д��ȥ
		phy_addr_t __addr = __combine_pst_reg_addr(pst_reg_array[lru_eviction_index].__tag);
		// �ӻ������
		read_cache((addr_t)__addr, pst_reg_array[lru_eviction_index].__data, CACHELINE);
		pst_reg_array[lru_eviction_index].__tag = tag;
		// ���ڴ����counter�Ƶ�extra�Ĵ�������ȥ����Ҫ��֤��
		reg_data_move(buffer, pst_reg_array[lru_eviction_index].__data, CACHELINE);
	}
	return CACHE_MISS;
}

status CPTA_t::__write(phy_addr_t addr, addr_t buffer, size_t n_bytes,
	eviction_cb_t eviction_cb) {
	//check addr cacheline align, byte_index = 0, n_bytes = CACHELINE
	phy_addr_t tag = __parse_pst_reg_tag(addr);
	size_t lru_eviction_index = 0;
	bool pst_reg_hit = false;

	for (size_t idx = 0; idx < N_CTR_PERSISTENT_REGISTER; idx++) {
		if (pst_reg_array[idx].__line_valid == VALID //��Чλ
			&& pst_reg_array[idx].__tag == tag) { //tag��ͬ
			pst_reg_hit = true;
			lru_eviction_index = idx;
			break;
		}
	}

	if (!pst_reg_hit) {
		select_pst_reg(&lru_eviction_index);
		if (pst_reg_array[lru_eviction_index].__dirty == DIRTY) {
			phy_addr_t __addr = __combine_pst_reg_addr(pst_reg_array[lru_eviction_index].__tag);
			if (eviction_cb) eviction_cb(__addr);
		}
	}

	pst_reg_array[lru_eviction_index].__tag = tag;
	pst_reg_array[lru_eviction_index].__dirty = DIRTY;
	reg_data_move(pst_reg_array[lru_eviction_index].__data, buffer, n_bytes);
	hit_fresh(lru_eviction_index);

	return pst_reg_hit ? CACHE_HIT : CACHE_MISS;
}

CPTA_t::Counter_Persistent_Register_Array() {
	memset(pst_reg_array, 0, sizeof(pst_reg_array));//��ʼ����0��ģ����
}

CPTA_t& g_CPTA = CPTA_t::get_instance();

status read_pst_reg(void* addr, void* buffer, size_t n_bytes, eviction_cb_t eviction_cb) {
	return g_CPTA.read(addr, buffer, n_bytes, eviction_cb);
}

status write_pst_reg(void* addr, void* buffer, size_t n_bytes,
	eviction_cb_t eviction_cb, bool write_back) {
	return g_CPTA.write(addr, buffer, n_bytes, eviction_cb, write_back);
}