#include "baseline_rw.h"

extern uint8_t* cacheline_register; //64B�����Ĵ���
extern uint8_t* cacheline_register_extra;

uint64_t upbound_exp_8(uint64_t value) {
	uint64_t __value = 8;
	while (__value < value) __value <<= 3;
	return __value;
}

status __update_bmt_path_baseline(phy_addr_t addr) {
	addr_t ctr_addr = 0;
	uint32_t __leaf_K = 0, __I = 0;

	BMT_get_ctr_attr_baseline(g_baseline_BMTree, addr, &ctr_addr, &__leaf_K, &__I);
	BMT_path_update(g_baseline_BMTree, __leaf_K, hash_func_background_update);
	return SUCCESS;
}

status parallel_udpate_to_metacache(phy_addr_t addr) {
	__update_bmt_path_baseline(addr);
	return SUCCESS;
}

status baseline_read(phy_addr_t addr, size_t n_bytes, Tick& read_latency) {
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t ctr_addr = 0, root = 0;
	uint32_t __BMT_K = 0, __leaf_K = 0, __I = 0;

	read_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_baseline(g_baseline_BMTree, __addr, &ctr_addr, &__leaf_K, &__I);
	status cache_op_status = read_cache(ctr_addr, cacheline_register_extra, CACHELINE, parallel_udpate_to_metacache);


 	if (cache_op_status != CACHE_HIT) {
		//��֤֮ǰ��Ҫ�����ݷŵ�cacheline_register,������������֤
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* ����������ý���֮��rootָ���cacheline�ᱻ���ص�cacheline_register,���Կ���������֤*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
			return FAILURE;
		}
	}

	//����OTP��ͬʱ��NVM�ж�ȡ����
	uint64_t major, minor;
	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);
	//��������(__addr, major, minor) (�����IV)����OTP���ܻ��߽����ˡ�
	//����OTP
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	//���ж�����ʱ��
	g_Statistics.t_lat_add(parallel_exec_extra_latency_unit);
	
	g_Statistics.inc_user_read_cntr();
	//++g_tick_clock
	//baseline���������
 	read_latency = g_Statistics.get_t_latency() - read_latency;

	return SUCCESS;
}

status baseline_write(phy_addr_t addr, size_t n_bytes, Tick& write_latency) {
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t ctr_addr = 0, root = 0;
	uint32_t __BMT_K = 0, __leaf_K = 0, __I = 0;

	write_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_baseline(g_baseline_BMTree, __addr, &ctr_addr, &__leaf_K, &__I);
	status cache_op_status = read_cache(ctr_addr, cacheline_register_extra, CACHELINE, parallel_udpate_to_metacache);

	//BMT��֤
	if (cache_op_status != CACHE_HIT) {
		//��֤֮ǰ��Ҫ�����ݷŵ�cacheline_register,������������֤
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* ����������ý���֮��rootָ���cacheline�ᱻ���ص�cacheline_register,���Կ���������֤*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
			return FAILURE;
		}
	}

	//counter����������OTP����
	uint64_t major, minor;

	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);

	//���ﴦ��ӽ��ܵ�����
	g_Statistics.inc_user_write_cntr();//д����
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	g_Statistics.t_lat_add(memory_write_latency_unit); //��������д�ص�ʱ��
	++minor;
	//���������

	if (register_union_ctr->set_K_minor(__I, minor) == MINOR_OVERFLOW) {
		//����������ﲻ֪���Ƿ������
	}

	//����д�أ�baselineд�������
	//bool write_back = ((minor % n_counter_write_back_threshold) == 0);
	//write_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);

	write_latency = g_Statistics.get_t_latency() - write_latency;
	//++g_tick_clock
	return SUCCESS;
}

status strict_persist_read(phy_addr_t addr, size_t n_bytes, Tick& read_latency) {
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t ctr_addr = 0, root = 0;
	uint32_t __BMT_K = 0, __leaf_K = 0, __I = 0;

	read_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_baseline(g_baseline_BMTree, __addr, &ctr_addr, &__leaf_K, &__I);
	status cache_op_status = read_cache(ctr_addr, cacheline_register_extra, CACHELINE);

	if (cache_op_status != CACHE_HIT) {
		//��֤֮ǰ��Ҫ�����ݷŵ�cacheline_register,������������֤
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* ����������ý���֮��rootָ���cacheline�ᱻ���ص�cacheline_register,���Կ���������֤*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat == VERIFY_SUCCESS) {
			// do nothing
		}
		else {
			return FAILURE;
		}
	}

	//����OTP��ͬʱ��NVM�ж�ȡ����
	uint64_t major, minor;
	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);
	//��������(__addr, major, minor) (�����IV)����OTP���ܻ��߽����ˡ�
	//����OTP
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	//���ж�����ʱ��
	g_Statistics.t_lat_add(parallel_memory_read_latency_unit);
	g_Statistics.inc_user_read_cntr();
	//++g_tick_clock
	//strict persist���������
	read_latency = g_Statistics.get_t_latency() - read_latency;

	return SUCCESS;
}


status __update_bmt_path_strict_persist(phy_addr_t addr) {
	addr_t ctr_addr = 0;
	uint32_t __BMT_K = 0, __leaf_K = 0, __I = 0;

	BMT_get_ctr_attr_baseline(g_baseline_BMTree, addr, &ctr_addr, &__leaf_K, &__I);
	BMT_path_update(g_baseline_BMTree, __leaf_K, hash_func_flush_update);
	return SUCCESS;
}

status strict_persist_write(phy_addr_t addr, size_t n_bytes, Tick& write_latency) {
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t ctr_addr = 0, root = 0;
	uint32_t __BMT_K = 0, __leaf_K = 0, __I = 0;

	write_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_baseline(g_baseline_BMTree, __addr, &ctr_addr, &__leaf_K, &__I);
	status cache_op_status = read_cache(ctr_addr, cacheline_register_extra, CACHELINE);

	//BMT��֤
	if (cache_op_status != CACHE_HIT) {
		//��֤֮ǰ��Ҫ�����ݷŵ�cacheline_register,������������֤
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* ����������ý���֮��rootָ���cacheline�ᱻ���ص�cacheline_register,���Կ���������֤*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat == VERIFY_SUCCESS) {
			// do nothing
		}
		else {
			return FAILURE;
		}
	}

	//counter����������OTP����
	uint64_t major, minor;

	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);

	//���ﴦ��ӽ��ܵ�����
	g_Statistics.inc_user_write_cntr();//д����
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	g_Statistics.t_lat_add(memory_write_latency_unit); //��������д�ص�ʱ��
	++minor;
	//���������

	if (register_union_ctr->set_K_minor(__I, minor) == MINOR_OVERFLOW) {
		//�������,�������ֱ�Ӽ��ӳ���? ��Ҫ����64��CTR��Ӧ�ĵ�ַ���ݣ�ȫ�����¼��ܡ�
	}

	//�ϸ�־û�������BMT����·��ֱ��root���߸��±�д��root
	__update_bmt_path_strict_persist(__addr);

	//����д�أ�baselineд�������
	//bool write_back = ((minor % n_counter_write_back_threshold) == 0);
	//memory_data_write(addr, buffer, CACHELINE);
	//����д�أ�baselineд�������
	//write_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
	write_latency = g_Statistics.get_t_latency() - write_latency;
	//����д�أ�strict persistд�������
	//++g_tick_clock
	return SUCCESS;
}

status __update_bmt_path_atomic_update(phy_addr_t addr) {
	addr_t ctr_addr = 0;
	uint32_t __BMT_K = 0, __leaf_K = 0, __I = 0;

	BMT_get_ctr_attr_baseline(g_baseline_BMTree, addr, &ctr_addr, &__leaf_K, &__I);
	BMT_path_update(g_baseline_BMTree, __leaf_K, hash_func_check_acc_update);
	return SUCCESS;
}

//��strict persist����ͬ
status atomic_update_read(phy_addr_t addr, size_t n_bytes, Tick& read_latency) {
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t ctr_addr = 0, root = 0;
	uint32_t __BMT_K = 0, __leaf_K = 0, __I = 0;
	bool use_acc_table = true;

	read_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_baseline(g_baseline_BMTree, __addr, &ctr_addr, &__leaf_K, &__I);
	status cache_op_status = read_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, use_acc_table);

	if (cache_op_status != CACHE_HIT) {
		//��֤֮ǰ��Ҫ�����ݷŵ�cacheline_register,������������֤
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* ����������ý���֮��rootָ���cacheline�ᱻ���ص�cacheline_register,���Կ���������֤*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
			// do nothing
	
			return FAILURE;
		}
	}

	//����OTP��ͬʱ��NVM�ж�ȡ����
	uint64_t major, minor;
	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);
	//��������(__addr, major, minor) (�����IV)����OTP���ܻ��߽����ˡ�
	//����OTP
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	//���ж�����ʱ��
	g_Statistics.t_lat_add(parallel_memory_read_latency_unit);
	g_Statistics.inc_user_read_cntr();
	//++g_tick_clock
	//atomic update���������

	read_latency = g_Statistics.get_t_latency() - read_latency;
	return SUCCESS;
}


status atomic_update_write(phy_addr_t addr, size_t n_bytes, Tick& write_latency) {
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t ctr_addr = 0, root = 0;
	uint32_t __BMT_K = 0, __leaf_K = 0, __I = 0;

	write_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_baseline(g_baseline_BMTree, __addr, &ctr_addr, &__leaf_K, &__I);
	status cache_op_status = read_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, true);

	//BMT��֤
	if (cache_op_status != CACHE_HIT) {
		//��֤֮ǰ��Ҫ�����ݷŵ�cacheline_register,������������֤
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* ����������ý���֮��rootָ���cacheline�ᱻ���ص�cacheline_register,���Կ���������֤*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
	
			return FAILURE;
		}
	}

	//counter����������OTP����
	uint64_t major, minor;

	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);

	//���ﴦ��ӽ��ܵ�����
	g_Statistics.inc_user_write_cntr();//д����
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	g_Statistics.t_lat_add(memory_write_latency_unit); //��������д�ص�ʱ��
	++minor;
	//���������

	if (register_union_ctr->set_K_minor(__I, minor) == MINOR_OVERFLOW) {
		//�������,��Atomic Update������ˢ������ļ�����
		//counterд��NVM��ͬʱacc_table����Ч
		bool write_back = ((minor % n_counter_write_back_threshold) == 0);
		write_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back, true);
	}


	//����BMT����·��ֱ��root����д�߸��ݶ�Ӧ��cache��ַ�����acc table
	__update_bmt_path_atomic_update(__addr);

	//����д�أ�atomic updateд�������
	//bool write_back = ((minor % n_counter_write_back_threshold) == 0);
	//memory_data_write(addr, buffer, CACHELINE);
	//write_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
	//����д��NVM��strict persistд�������

	write_latency = g_Statistics.get_t_latency() - write_latency;

	//++g_tick_clock
	return SUCCESS;
}

