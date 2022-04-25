#include "baseline_rw.h"

extern uint8_t* cacheline_register; //64B共享大寄存器
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
		//验证之前需要把数据放到cacheline_register,传副本进行验证
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* 这个函数调用结束之后，root指向的cacheline会被加载到cacheline_register,所以可以连续验证*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
			return FAILURE;
		}
	}

	//计算OTP，同时从NVM中读取数据
	uint64_t major, minor;
	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);
	//可以利用(__addr, major, minor) (这个是IV)计算OTP加密或者解密了。
	//计算OTP
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	//并行读数据时延
	g_Statistics.t_lat_add(parallel_exec_extra_latency_unit);
	
	g_Statistics.inc_user_read_cntr();
	//++g_tick_clock
	//baseline读请求完成
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

	//BMT验证
	if (cache_op_status != CACHE_HIT) {
		//验证之前需要把数据放到cacheline_register,传副本进行验证
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* 这个函数调用结束之后，root指向的cacheline会被加载到cacheline_register,所以可以连续验证*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
			return FAILURE;
		}
	}

	//counter自增，生成OTP加密
	uint64_t major, minor;

	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);

	//这里处理加解密的事情
	g_Statistics.inc_user_write_cntr();//写次数
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	g_Statistics.t_lat_add(memory_write_latency_unit); //加密数据写回的时延
	++minor;
	//处理完加密

	if (register_union_ctr->set_K_minor(__I, minor) == MINOR_OVERFLOW) {
		//溢出处理，这里不知道是否处理溢出
	}

	//密文写回，baseline写请求完成
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
		//验证之前需要把数据放到cacheline_register,传副本进行验证
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* 这个函数调用结束之后，root指向的cacheline会被加载到cacheline_register,所以可以连续验证*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat == VERIFY_SUCCESS) {
			// do nothing
		}
		else {
			return FAILURE;
		}
	}

	//计算OTP，同时从NVM中读取数据
	uint64_t major, minor;
	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);
	//可以利用(__addr, major, minor) (这个是IV)计算OTP加密或者解密了。
	//计算OTP
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	//并行读数据时延
	g_Statistics.t_lat_add(parallel_memory_read_latency_unit);
	g_Statistics.inc_user_read_cntr();
	//++g_tick_clock
	//strict persist读请求完成
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

	//BMT验证
	if (cache_op_status != CACHE_HIT) {
		//验证之前需要把数据放到cacheline_register,传副本进行验证
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* 这个函数调用结束之后，root指向的cacheline会被加载到cacheline_register,所以可以连续验证*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat == VERIFY_SUCCESS) {
			// do nothing
		}
		else {
			return FAILURE;
		}
	}

	//counter自增，生成OTP加密
	uint64_t major, minor;

	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);

	//这里处理加解密的事情
	g_Statistics.inc_user_write_cntr();//写次数
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	g_Statistics.t_lat_add(memory_write_latency_unit); //加密数据写回的时延
	++minor;
	//处理完加密

	if (register_union_ctr->set_K_minor(__I, minor) == MINOR_OVERFLOW) {
		//溢出处理,这里可以直接加延迟吗? 需要把这64个CTR对应的地址数据，全部重新加密。
	}

	//严格持久化，更新BMT整条路径直到root，边更新边写回root
	__update_bmt_path_strict_persist(__addr);

	//密文写回，baseline写请求完成
	//bool write_back = ((minor % n_counter_write_back_threshold) == 0);
	//memory_data_write(addr, buffer, CACHELINE);
	//密文写回，baseline写请求完成
	//write_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
	write_latency = g_Statistics.get_t_latency() - write_latency;
	//密文写回，strict persist写请求完成
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

//和strict persist都相同
status atomic_update_read(phy_addr_t addr, size_t n_bytes, Tick& read_latency) {
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t ctr_addr = 0, root = 0;
	uint32_t __BMT_K = 0, __leaf_K = 0, __I = 0;
	bool use_acc_table = true;

	read_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_baseline(g_baseline_BMTree, __addr, &ctr_addr, &__leaf_K, &__I);
	status cache_op_status = read_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, use_acc_table);

	if (cache_op_status != CACHE_HIT) {
		//验证之前需要把数据放到cacheline_register,传副本进行验证
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* 这个函数调用结束之后，root指向的cacheline会被加载到cacheline_register,所以可以连续验证*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
			// do nothing
	
			return FAILURE;
		}
	}

	//计算OTP，同时从NVM中读取数据
	uint64_t major, minor;
	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);
	//可以利用(__addr, major, minor) (这个是IV)计算OTP加密或者解密了。
	//计算OTP
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	//并行读数据时延
	g_Statistics.t_lat_add(parallel_memory_read_latency_unit);
	g_Statistics.inc_user_read_cntr();
	//++g_tick_clock
	//atomic update读请求完成

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

	//BMT验证
	if (cache_op_status != CACHE_HIT) {
		//验证之前需要把数据放到cacheline_register,传副本进行验证
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		/* 这个函数调用结束之后，root指向的cacheline会被加载到cacheline_register,所以可以连续验证*/
		status stat = BMT_verify_counter(g_baseline_BMTree, __leaf_K, __BMT_K, root);
		if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
	
			return FAILURE;
		}
	}

	//counter自增，生成OTP加密
	uint64_t major, minor;

	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);

	//这里处理加解密的事情
	g_Statistics.inc_user_write_cntr();//写次数
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	g_Statistics.t_lat_add(memory_write_latency_unit); //加密数据写回的时延
	++minor;
	//处理完加密

	if (register_union_ctr->set_K_minor(__I, minor) == MINOR_OVERFLOW) {
		//溢出处理,在Atomic Update方案中刷回溢出的计数器
		//counter写回NVM，同时acc_table置无效
		bool write_back = ((minor % n_counter_write_back_threshold) == 0);
		write_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back, true);
	}


	//更新BMT整条路径直到root，边写边根据对应的cache地址，检查acc table
	__update_bmt_path_atomic_update(__addr);

	//密文写回，atomic update写请求完成
	//bool write_back = ((minor % n_counter_write_back_threshold) == 0);
	//memory_data_write(addr, buffer, CACHELINE);
	//write_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
	//密文写回NVM，strict persist写请求完成

	write_latency = g_Statistics.get_t_latency() - write_latency;

	//++g_tick_clock
	return SUCCESS;
}

