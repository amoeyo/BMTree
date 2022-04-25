#include "promt_np_rw.h"
 

extern uint8_t* cacheline_register; //64B共享大寄存器
extern uint8_t* cacheline_register_extra;

page_addr_t g_hotpage_map[PROMT_HOT_REAL_LEAF_N + 1]; //hot tree到page的映射，直接通过hot tree index索引到page地址

extern uint64_t g_valid_leaf_N = 1;

extern ds_addr_t DescriptorBlockIdxTable;

#define PRO_DEBUG

#ifdef PROMT_DEBUG
void print_hotpage_map() {
	for (int i = 0; i < PROMT_HOT_REAL_LEAF_N; i++) {
		if (g_hotpage_map[i]) {
			fmt_print_k_hex_v(i, g_hotpage_map[i]);
		}
	}
}

void print_cacheline_reg() {
	addr_t p = cacheline_register;
	printf("cacheline_reg: ");
	for (int k = 0; k < 64; k++) {
		printf("%02x", p[k]);
	}
	printf("\n");
}

void print_cacheline_reg_extra() {
	addr_t p = cacheline_register_extra;
	printf("cacheline_reg_extra: ");
	for (int k = 0; k < 64; k++) {
		printf("%02x", p[k]);
	}
	printf("\n");
}
#endif

void copy_attr_from_global_to_hot(uint32_t hot_leaf_K, uint32_t global_leaf_K) {
	addr_t global_addr, hot_addr;

	cntr_t hit, miss, extra_read, extra_write, eviction;

	hit = g_Statistics.get_cache_hit_cntr();
	miss = g_Statistics.get_cache_miss_cntr();
	extra_read = g_Statistics.get_meta_extra_write_cntr();
	extra_write = g_Statistics.get_meta_extra_read_cntr();
	eviction = g_Statistics.get_meta_cache_eviction_cntr();

	BMT_get_leaf_K_addr(g_global_BMTree, global_leaf_K, &global_addr);
	BMT_get_leaf_K_addr(g_hot_BMTree, hot_leaf_K, &hot_addr);
	read_cache(global_addr, cacheline_register, CACHELINE);
	write_cache(hot_addr, cacheline_register, CACHELINE);

	g_Statistics.set_cache_hit_cntr(hit);
	g_Statistics.set_cache_miss_cntr(miss);
	g_Statistics.set_meta_extra_write_cntr(extra_read);
	g_Statistics.set_meta_extra_read_cntr(extra_read);
	g_Statistics.set_meta_cache_eviction_cntr(eviction);
}

void copy_attr_from_hot_to_global(uint32_t global_leaf_K, uint32_t hot_leaf_K) {
	addr_t global_addr, hot_addr;

	cntr_t hit, miss, extra_read, extra_write, eviction;

	hit = g_Statistics.get_cache_hit_cntr();
	miss = g_Statistics.get_cache_miss_cntr();
	extra_read = g_Statistics.get_meta_extra_write_cntr();
	extra_write = g_Statistics.get_meta_extra_read_cntr();
	eviction = g_Statistics.get_meta_cache_eviction_cntr();

	BMT_get_leaf_K_addr(g_global_BMTree, global_leaf_K, &global_addr);
	BMT_get_leaf_K_addr(g_hot_BMTree, hot_leaf_K, &hot_addr);
	read_cache(hot_addr, cacheline_register, CACHELINE);
	write_cache(global_addr, cacheline_register, CACHELINE);

	g_Statistics.set_cache_hit_cntr(hit);
	g_Statistics.set_cache_miss_cntr(miss);
	g_Statistics.set_meta_extra_write_cntr(extra_read);
	g_Statistics.set_meta_extra_read_cntr(extra_read);
	g_Statistics.set_meta_cache_eviction_cntr(eviction);
}


//ProMT-NP方案
Tick promt_np_read(phy_addr_t addr, size_t n_bytes, Tick& read_latency) {
	//根据地址先获取Ctr
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t ctr_addr = 0; //这需要根据__addr计算区块，然后查表，获得偏移，计算在全局树中的叶子的位置。
	uint32_t __leaf_K = 0, __I = 0;

	read_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_global(g_global_BMTree, __addr, &ctr_addr, &__leaf_K, &__I);
	status cache_op_status = read_cache(ctr_addr, cacheline_register_extra, CACHELINE);	//读缓存
	//check cache_hit != PARAMETER_ERROR

	if (cache_op_status != CACHE_HIT) {
		page_addr_t page_addr = __leaf_K;
		ds_addr_t cur_ds = DescriptorBlockIdxTable + page_addr;
		ds_blk_t ds_register; //寄存器
		read_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));

		uint64_t hot_index;
		// 获取Padding/Hot EC
		ds_register.get_hot_addr(&hot_index);
		//传副本进行验证
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		if (hot_index) {
			//通过hot tree验证，page_addr指示根据hot tree叶子索引得到的页地址，但同时也是region tree的leaf_K
			status stat = BMT_verify_counter(g_hot_BMTree, hot_index - 1, 0, g_hot_BMTree_root);
			if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
				return FAILURE;
			}
			// 进一步处理，如果有的话
		}
		else {
			status stat = BMT_verify_counter(g_global_BMTree, __leaf_K, 0, g_global_BMTree_root);
			if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
				return FAILURE;
			}
			// 进一步处理，如果有的话
		}
	}

	//计算OTP，同时从NVM中读取数据
	uint64_t major, minor;
	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);
	//可以利用(__addr, major, minor) (这个是IV)计算OTP加密或者解密了。
	//  
	//计算OTP
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	//并行读数据时延
	g_Statistics.t_lat_add(parallel_exec_extra_latency_unit);
	g_Statistics.inc_user_read_cntr();
	//++g_tick_clock

	read_latency = g_Statistics.get_t_latency() - read_latency;
#ifdef PROMT_DEBUG
	//print_mq();
#endif
	//ProMT-NP读请求完成
	return g_Statistics.get_t_latency();
}

//后台进程，调整MQ
status update_g_multiqueue()
{
	//TODO每进行100次内存写，调整mq
	if (g_Statistics.get_mem_write_cycle_cntr() < 100) return SUCCESS;
	g_Statistics.clear_mem_write_cycle_cntr();
	ds_blk_t ds_register;
	uint64_t exp_time, demo_flag, queue_num, next_frame, page_addr;
	for (int i = 0; i < MULTIQUEUE_SIZE; i++) {
		// 前提是此时的队列不为空
		if (get_queue_idx_size(i)) {
			// 读了一次缓存
			get_queue_head(i, &ds_register);
			ds_register.get_page_addr(&page_addr);
			ds_register.get_queue_num(&queue_num);
			ds_register.get_exp_time(&exp_time);
			ds_register.get_demo_flag(&demo_flag);
			ds_register.get_frame_pointer(&next_frame);
			if (exp_time < g_Statistics.get_total_write_cntr()) {
				// 从头部踢出来，仅修改头部frame pointer
				evict_queue_head(queue_num, &ds_register);
				ds_register.set_frame_pointer(NULL_TAIL);
				if (demo_flag) {
					// 两次降级，如果此时queue_num=1，说明是从Q2降下来的，可以挂到Q3最后
					// 如果此时queue_num=0，说明是从Q1降下来的，这个应该不能挂到Q3最后
					if (queue_num > 0) {
						push_back_queue_idx(&ds_register, VICTIM_QUEUE_INDEX);
						ds_register.set_queue_num(VICTIM_QUEUE_INDEX);
					}
					else {
						// 这里是Q0首部也就是原属Q1的第二次降级，直接踢出去
						ds_register.clear();
					}
				}
				else {
					//修改demo flag，降级
					ds_register.set_demo_flag(1);
					if (queue_num > 0) {
						// 可以降一级，Q1降到Q0，Q2降到Q1，再降的话就会被抬到Q3后面
						queue_num--;
						push_back_queue_idx(&ds_register, queue_num);
						ds_register.set_queue_num(queue_num);
					}
					else {
						// Q0首部没法降级，踢出MQ
						ds_register.clear();
					}
				}
				// 写回？
				write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
			}
		}
	}
	return SUCCESS;
}



Tick promt_np_write(phy_addr_t addr, size_t n_bytes, Tick& write_latency) {
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t hot_ctr_addr = 0, global_ctr_addr = 0; //这需要根据__addr计算区块，然后查表，获得偏移，计算在全局树中的叶子的位置。
	uint32_t global_leaf_K = 0, __I = 0, hot_leaf_K = 0;

	check_sq_size();

	write_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_global(g_global_BMTree, __addr, &global_ctr_addr, &global_leaf_K, &__I);

	page_addr_t page_addr = global_leaf_K;
	ds_addr_t cur_ds = DescriptorBlockIdxTable + page_addr;
	ds_blk_t ds_register; //寄存器
	read_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));
#ifdef PROMT_DEBUG
	//ds_register.print();
#endif
	uint64_t acc_num, exp_time, queue_num, hot_index;
	ds_register.get_acc_num(&acc_num);
	ds_register.get_exp_time(&exp_time);
	ds_register.get_queue_num(&queue_num);

	// 获取Padding/Hot EC
	ds_register.get_hot_addr(&hot_index);
	acc_num++;
	exp_time = g_Statistics.get_total_write_cntr() + LIFETIME;
	ds_register.set_acc_num(acc_num);
	ds_register.set_exp_time(exp_time);
	ds_register.set_demo_flag(0);
	// 这个函数退出后，ds_register的尾指针已经改成正确的了，但是这里暂时不写回
	push_queue_idx(&ds_register, &queue_num);
	ds_register.set_queue_num(queue_num);
	ds_register.set_frame_pointer(NULL_TAIL);

	if (hot_index) {
		// hot_index记录计数器在hot tree叶子节点的位置
		hot_leaf_K = hot_index - 1;
		BMT_get_leaf_K_addr(g_hot_BMTree, hot_leaf_K, &hot_ctr_addr);
		status cache_op_status = read_cache(hot_ctr_addr, cacheline_register_extra, CACHELINE);	//读缓存
		
		if (cache_op_status != CACHE_HIT) { //通过hot tree进行验证
			reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
			status stat = BMT_verify_counter(g_hot_BMTree, hot_leaf_K, 0, g_hot_BMTree_root);
			if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
				return FAILURE;
			}
		}

		uint64_t major, minor;

		RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
		register_union_ctr->getValue(__I, &major, &minor);

		//counter + 1
		++minor;
		if (register_union_ctr->set_K_minor(__I, minor) == MINOR_OVERFLOW) {
			//溢出处理,这里可以直接加延迟吗? 需要把这64个CTR对应的地址数据，全部重新加密。
		}

		bool write_back = ((minor % n_counter_write_back_threshold) == 0);
		write_cache(hot_ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
		if (write_back) {
			g_Statistics.inc_counter_overflow_write_cntr();
		}
		//更新hot-tree到root
		BMT_path_update(g_hot_BMTree, hot_leaf_K, 0, g_hot_BMTree_root);
		write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
	}
	else {
		// 直接用global_ctr_addr，global_leaf_K，__I
		status cache_op_status = read_cache(global_ctr_addr, cacheline_register_extra, CACHELINE);	//读缓存
		if (cache_op_status != CACHE_HIT) { //验证global tree
			reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
			status stat = BMT_verify_counter(g_global_BMTree, global_leaf_K, 0, g_global_BMTree_root);
			if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
				return FAILURE;
			}
		}
		uint64_t major, minor;

		RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
		register_union_ctr->getValue(__I, &major, &minor);

		//counter + 1
		++minor;
		if (register_union_ctr->set_K_minor(__I, minor) == MINOR_OVERFLOW) {
			//溢出处理,这里可以直接加延迟吗? 需要把这64个CTR对应的地址数据，全部重新加密。
		}
		bool write_back = ((minor % n_counter_write_back_threshold) == 0);
		if (write_back) {
			g_Statistics.inc_counter_overflow_write_cntr();
		}

		if (acc_num == 4) {
			//hot tree未满，此时ds已经push到Q2队尾了
			if (g_valid_leaf_N <= PROMT_HOT_REAL_LEAF_N) {
				// 更新（增加）映射
				g_hotpage_map[g_valid_leaf_N] = global_leaf_K;
				hot_index = g_valid_leaf_N;
				// 设置当前ds的Padding/Hot EC
				ds_register.set_hot_addr(hot_index);
				// 更新hot tree，叶子节点在当前的global tree当中
				// 因此需要将节点从global tree copy到hot tree，因此造成的读写次数和延迟要还原
				hot_leaf_K = hot_index - 1;
				// 将global tree上的节点copy到hot tree
				copy_attr_from_global_to_hot(hot_leaf_K, global_leaf_K);
				BMT_get_leaf_K_addr(g_hot_BMTree, hot_leaf_K, &hot_ctr_addr);
				write_cache(hot_ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
				
				// 更新到hot tree
				BMT_path_update(g_hot_BMTree, hot_leaf_K, 0, g_hot_BMTree_root);
				// 将更新后的ds写回缓存
				write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
				g_valid_leaf_N++;
			}
			else {
				// 获取Q3的第一个page地址，在这个步骤对应page的ds已经从缓存中读出来，清零并写缓存，Q3头指针修改
				if (get_queue_idx_size(VICTIM_QUEUE_INDEX)) {
					page_addr_t victim_leaf_K;
					uint64_t victim_hot_index;
					ds_blk_t victim_ds;

					//print_sq_idx(VICTIM_QUEUE_INDEX);

					evict_queue_head(VICTIM_QUEUE_INDEX, &victim_ds);

					victim_ds.get_hot_addr(&victim_hot_index);
					//victim_leaf_K = g_hotpage_map[victim_hot_index];
					victim_ds.get_page_addr(&victim_leaf_K);
				    // 从cache中读对应page的node，64B，对它做整体的验证
					addr_t victim_global_addr, victim_hot_addr;
					//BMT_get_leaf_K_addr(g_global_BMTree, victim_leaf_K, &victim_addr);
					//victim node 在hot tree上的位置
					BMT_get_leaf_K_addr(g_hot_BMTree, victim_hot_index - 1, &victim_hot_addr);
					//victim node 在global tree上的位置
					BMT_get_leaf_K_addr(g_global_BMTree, victim_leaf_K, &victim_global_addr);

					if (victim_hot_index) {
						// victim node在hot tree上
						status victim_stat = read_cache(victim_hot_addr, cacheline_register, CACHELINE);
						if (victim_stat != CACHE_HIT) {
							status stat = BMT_verify_counter(g_hot_BMTree, victim_hot_index - 1, 0, g_hot_BMTree_root);
							if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
								printf("hot verify failure, write cntr: %d, victim_leaf_K: %d\n", g_Statistics.get_user_write_cntr(), victim_leaf_K);
								return FAILURE;
							}	
						}
						// 验证无误后更新hot tree的victim节点到global tree
						// 这里的延迟要减掉?
						copy_attr_from_hot_to_global(victim_leaf_K, victim_hot_index - 1);
						// 验证无误后更新global tree，严格持久化9*150延迟
						BMT_path_update(g_global_BMTree, victim_leaf_K, hash_func_flush_update);
						//更新对应DS在hot tree中的位置，这里占了victim node的位置，而且要求写回并覆盖
						g_hotpage_map[victim_hot_index] = global_leaf_K;
						ds_register.set_hot_addr(victim_hot_index);
						// 当前写请求对应的counter更新到hot tree
						// 要注意这里的hot_index是没有值的！！应该是更新到被替换的hot tree leaf
						// 这里要把目前正在读的计数器copy（加入）到hot tree，也就是从global读出来然后写入hot
						// 这块算作拷贝延迟，因此造成的read/write次数和延迟都要减掉
						copy_attr_from_global_to_hot(victim_hot_index - 1, global_leaf_K);
						BMT_path_update(g_hot_BMTree, victim_hot_index - 1, 0, g_hot_BMTree_root);
					}
					else {
						// 当前q3头结点没有在hot tree里面，无法替换，仅驱逐，do nothing
						write_cache(global_ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
						//严格持久化global-tree到root
						BMT_path_update(g_global_BMTree, global_leaf_K, hash_func_flush_update);
					}
					write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t), true);
				}
				else {
					write_cache(global_ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
					//严格持久化global-tree到root
					BMT_path_update(g_global_BMTree, global_leaf_K, hash_func_flush_update);
					write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
				}
			}
		}
		else { // counter+1 严格持久化
			write_cache(global_ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
			//严格持久化global-tree到root
			BMT_path_update(g_global_BMTree, global_leaf_K, hash_func_flush_update);
			write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
		}
	}
	
	//这里处理加解密的事情
	g_Statistics.inc_user_write_cntr();
	g_Statistics.inc_mem_write_cycle_cntr();
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	g_Statistics.t_lat_add(memory_write_latency_unit); //加密数据写回的时延
	//处理完加密

	latency_t start_update = g_Statistics.get_t_latency();

	//后台检测，因此造成的延迟要隐藏
	update_g_multiqueue();

	g_Statistics.set_t_latency(start_update);

	latency_t update_lat = g_Statistics.get_t_latency() - start_update;

	write_latency = g_Statistics.get_t_latency() - write_latency - update_lat;

#ifdef PROMT_DEBUG
	//print_hotpage_map();
	//print_mq();
	check_sq_size();
#endif
	return start_update;
	//ProMT-NP写请求完成
}



