#include "promt_np_rw.h"
 

extern uint8_t* cacheline_register; //64B�����Ĵ���
extern uint8_t* cacheline_register_extra;

page_addr_t g_hotpage_map[PROMT_HOT_REAL_LEAF_N + 1]; //hot tree��page��ӳ�䣬ֱ��ͨ��hot tree index������page��ַ

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


//ProMT-NP����
Tick promt_np_read(phy_addr_t addr, size_t n_bytes, Tick& read_latency) {
	//���ݵ�ַ�Ȼ�ȡCtr
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t ctr_addr = 0; //����Ҫ����__addr�������飬Ȼ�������ƫ�ƣ�������ȫ�����е�Ҷ�ӵ�λ�á�
	uint32_t __leaf_K = 0, __I = 0;

	read_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_global(g_global_BMTree, __addr, &ctr_addr, &__leaf_K, &__I);
	status cache_op_status = read_cache(ctr_addr, cacheline_register_extra, CACHELINE);	//������
	//check cache_hit != PARAMETER_ERROR

	if (cache_op_status != CACHE_HIT) {
		page_addr_t page_addr = __leaf_K;
		ds_addr_t cur_ds = DescriptorBlockIdxTable + page_addr;
		ds_blk_t ds_register; //�Ĵ���
		read_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));

		uint64_t hot_index;
		// ��ȡPadding/Hot EC
		ds_register.get_hot_addr(&hot_index);
		//������������֤
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		if (hot_index) {
			//ͨ��hot tree��֤��page_addrָʾ����hot treeҶ�������õ���ҳ��ַ����ͬʱҲ��region tree��leaf_K
			status stat = BMT_verify_counter(g_hot_BMTree, hot_index - 1, 0, g_hot_BMTree_root);
			if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
				return FAILURE;
			}
			// ��һ����������еĻ�
		}
		else {
			status stat = BMT_verify_counter(g_global_BMTree, __leaf_K, 0, g_global_BMTree_root);
			if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
				return FAILURE;
			}
			// ��һ����������еĻ�
		}
	}

	//����OTP��ͬʱ��NVM�ж�ȡ����
	uint64_t major, minor;
	RegisterUnionCtr* register_union_ctr = (RegisterUnionCtr*)cacheline_register_extra;
	register_union_ctr->getValue(__I, &major, &minor);
	//��������(__addr, major, minor) (�����IV)����OTP���ܻ��߽����ˡ�
	//  
	//����OTP
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	//���ж�����ʱ��
	g_Statistics.t_lat_add(parallel_exec_extra_latency_unit);
	g_Statistics.inc_user_read_cntr();
	//++g_tick_clock

	read_latency = g_Statistics.get_t_latency() - read_latency;
#ifdef PROMT_DEBUG
	//print_mq();
#endif
	//ProMT-NP���������
	return g_Statistics.get_t_latency();
}

//��̨���̣�����MQ
status update_g_multiqueue()
{
	//TODOÿ����100���ڴ�д������mq
	if (g_Statistics.get_mem_write_cycle_cntr() < 100) return SUCCESS;
	g_Statistics.clear_mem_write_cycle_cntr();
	ds_blk_t ds_register;
	uint64_t exp_time, demo_flag, queue_num, next_frame, page_addr;
	for (int i = 0; i < MULTIQUEUE_SIZE; i++) {
		// ǰ���Ǵ�ʱ�Ķ��в�Ϊ��
		if (get_queue_idx_size(i)) {
			// ����һ�λ���
			get_queue_head(i, &ds_register);
			ds_register.get_page_addr(&page_addr);
			ds_register.get_queue_num(&queue_num);
			ds_register.get_exp_time(&exp_time);
			ds_register.get_demo_flag(&demo_flag);
			ds_register.get_frame_pointer(&next_frame);
			if (exp_time < g_Statistics.get_total_write_cntr()) {
				// ��ͷ���߳��������޸�ͷ��frame pointer
				evict_queue_head(queue_num, &ds_register);
				ds_register.set_frame_pointer(NULL_TAIL);
				if (demo_flag) {
					// ���ν����������ʱqueue_num=1��˵���Ǵ�Q2�������ģ����Թҵ�Q3���
					// �����ʱqueue_num=0��˵���Ǵ�Q1�������ģ����Ӧ�ò��ܹҵ�Q3���
					if (queue_num > 0) {
						push_back_queue_idx(&ds_register, VICTIM_QUEUE_INDEX);
						ds_register.set_queue_num(VICTIM_QUEUE_INDEX);
					}
					else {
						// ������Q0�ײ�Ҳ����ԭ��Q1�ĵڶ��ν�����ֱ���߳�ȥ
						ds_register.clear();
					}
				}
				else {
					//�޸�demo flag������
					ds_register.set_demo_flag(1);
					if (queue_num > 0) {
						// ���Խ�һ����Q1����Q0��Q2����Q1���ٽ��Ļ��ͻᱻ̧��Q3����
						queue_num--;
						push_back_queue_idx(&ds_register, queue_num);
						ds_register.set_queue_num(queue_num);
					}
					else {
						// Q0�ײ�û���������߳�MQ
						ds_register.clear();
					}
				}
				// д�أ�
				write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
			}
		}
	}
	return SUCCESS;
}



Tick promt_np_write(phy_addr_t addr, size_t n_bytes, Tick& write_latency) {
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t hot_ctr_addr = 0, global_ctr_addr = 0; //����Ҫ����__addr�������飬Ȼ�������ƫ�ƣ�������ȫ�����е�Ҷ�ӵ�λ�á�
	uint32_t global_leaf_K = 0, __I = 0, hot_leaf_K = 0;

	check_sq_size();

	write_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_global(g_global_BMTree, __addr, &global_ctr_addr, &global_leaf_K, &__I);

	page_addr_t page_addr = global_leaf_K;
	ds_addr_t cur_ds = DescriptorBlockIdxTable + page_addr;
	ds_blk_t ds_register; //�Ĵ���
	read_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));
#ifdef PROMT_DEBUG
	//ds_register.print();
#endif
	uint64_t acc_num, exp_time, queue_num, hot_index;
	ds_register.get_acc_num(&acc_num);
	ds_register.get_exp_time(&exp_time);
	ds_register.get_queue_num(&queue_num);

	// ��ȡPadding/Hot EC
	ds_register.get_hot_addr(&hot_index);
	acc_num++;
	exp_time = g_Statistics.get_total_write_cntr() + LIFETIME;
	ds_register.set_acc_num(acc_num);
	ds_register.set_exp_time(exp_time);
	ds_register.set_demo_flag(0);
	// ��������˳���ds_register��βָ���Ѿ��ĳ���ȷ���ˣ�����������ʱ��д��
	push_queue_idx(&ds_register, &queue_num);
	ds_register.set_queue_num(queue_num);
	ds_register.set_frame_pointer(NULL_TAIL);

	if (hot_index) {
		// hot_index��¼��������hot treeҶ�ӽڵ��λ��
		hot_leaf_K = hot_index - 1;
		BMT_get_leaf_K_addr(g_hot_BMTree, hot_leaf_K, &hot_ctr_addr);
		status cache_op_status = read_cache(hot_ctr_addr, cacheline_register_extra, CACHELINE);	//������
		
		if (cache_op_status != CACHE_HIT) { //ͨ��hot tree������֤
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
			//�������,�������ֱ�Ӽ��ӳ���? ��Ҫ����64��CTR��Ӧ�ĵ�ַ���ݣ�ȫ�����¼��ܡ�
		}

		bool write_back = ((minor % n_counter_write_back_threshold) == 0);
		write_cache(hot_ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
		if (write_back) {
			g_Statistics.inc_counter_overflow_write_cntr();
		}
		//����hot-tree��root
		BMT_path_update(g_hot_BMTree, hot_leaf_K, 0, g_hot_BMTree_root);
		write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
	}
	else {
		// ֱ����global_ctr_addr��global_leaf_K��__I
		status cache_op_status = read_cache(global_ctr_addr, cacheline_register_extra, CACHELINE);	//������
		if (cache_op_status != CACHE_HIT) { //��֤global tree
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
			//�������,�������ֱ�Ӽ��ӳ���? ��Ҫ����64��CTR��Ӧ�ĵ�ַ���ݣ�ȫ�����¼��ܡ�
		}
		bool write_back = ((minor % n_counter_write_back_threshold) == 0);
		if (write_back) {
			g_Statistics.inc_counter_overflow_write_cntr();
		}

		if (acc_num == 4) {
			//hot treeδ������ʱds�Ѿ�push��Q2��β��
			if (g_valid_leaf_N <= PROMT_HOT_REAL_LEAF_N) {
				// ���£����ӣ�ӳ��
				g_hotpage_map[g_valid_leaf_N] = global_leaf_K;
				hot_index = g_valid_leaf_N;
				// ���õ�ǰds��Padding/Hot EC
				ds_register.set_hot_addr(hot_index);
				// ����hot tree��Ҷ�ӽڵ��ڵ�ǰ��global tree����
				// �����Ҫ���ڵ��global tree copy��hot tree�������ɵĶ�д�������ӳ�Ҫ��ԭ
				hot_leaf_K = hot_index - 1;
				// ��global tree�ϵĽڵ�copy��hot tree
				copy_attr_from_global_to_hot(hot_leaf_K, global_leaf_K);
				BMT_get_leaf_K_addr(g_hot_BMTree, hot_leaf_K, &hot_ctr_addr);
				write_cache(hot_ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
				
				// ���µ�hot tree
				BMT_path_update(g_hot_BMTree, hot_leaf_K, 0, g_hot_BMTree_root);
				// �����º��dsд�ػ���
				write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
				g_valid_leaf_N++;
			}
			else {
				// ��ȡQ3�ĵ�һ��page��ַ������������Ӧpage��ds�Ѿ��ӻ����ж����������㲢д���棬Q3ͷָ���޸�
				if (get_queue_idx_size(VICTIM_QUEUE_INDEX)) {
					page_addr_t victim_leaf_K;
					uint64_t victim_hot_index;
					ds_blk_t victim_ds;

					//print_sq_idx(VICTIM_QUEUE_INDEX);

					evict_queue_head(VICTIM_QUEUE_INDEX, &victim_ds);

					victim_ds.get_hot_addr(&victim_hot_index);
					//victim_leaf_K = g_hotpage_map[victim_hot_index];
					victim_ds.get_page_addr(&victim_leaf_K);
				    // ��cache�ж���Ӧpage��node��64B���������������֤
					addr_t victim_global_addr, victim_hot_addr;
					//BMT_get_leaf_K_addr(g_global_BMTree, victim_leaf_K, &victim_addr);
					//victim node ��hot tree�ϵ�λ��
					BMT_get_leaf_K_addr(g_hot_BMTree, victim_hot_index - 1, &victim_hot_addr);
					//victim node ��global tree�ϵ�λ��
					BMT_get_leaf_K_addr(g_global_BMTree, victim_leaf_K, &victim_global_addr);

					if (victim_hot_index) {
						// victim node��hot tree��
						status victim_stat = read_cache(victim_hot_addr, cacheline_register, CACHELINE);
						if (victim_stat != CACHE_HIT) {
							status stat = BMT_verify_counter(g_hot_BMTree, victim_hot_index - 1, 0, g_hot_BMTree_root);
							if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
								printf("hot verify failure, write cntr: %d, victim_leaf_K: %d\n", g_Statistics.get_user_write_cntr(), victim_leaf_K);
								return FAILURE;
							}	
						}
						// ��֤��������hot tree��victim�ڵ㵽global tree
						// ������ӳ�Ҫ����?
						copy_attr_from_hot_to_global(victim_leaf_K, victim_hot_index - 1);
						// ��֤��������global tree���ϸ�־û�9*150�ӳ�
						BMT_path_update(g_global_BMTree, victim_leaf_K, hash_func_flush_update);
						//���¶�ӦDS��hot tree�е�λ�ã�����ռ��victim node��λ�ã�����Ҫ��д�ز�����
						g_hotpage_map[victim_hot_index] = global_leaf_K;
						ds_register.set_hot_addr(victim_hot_index);
						// ��ǰд�����Ӧ��counter���µ�hot tree
						// Ҫע�������hot_index��û��ֵ�ģ���Ӧ���Ǹ��µ����滻��hot tree leaf
						// ����Ҫ��Ŀǰ���ڶ��ļ�����copy�����룩��hot tree��Ҳ���Ǵ�global������Ȼ��д��hot
						// ������������ӳ٣������ɵ�read/write�������ӳٶ�Ҫ����
						copy_attr_from_global_to_hot(victim_hot_index - 1, global_leaf_K);
						BMT_path_update(g_hot_BMTree, victim_hot_index - 1, 0, g_hot_BMTree_root);
					}
					else {
						// ��ǰq3ͷ���û����hot tree���棬�޷��滻��������do nothing
						write_cache(global_ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
						//�ϸ�־û�global-tree��root
						BMT_path_update(g_global_BMTree, global_leaf_K, hash_func_flush_update);
					}
					write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t), true);
				}
				else {
					write_cache(global_ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
					//�ϸ�־û�global-tree��root
					BMT_path_update(g_global_BMTree, global_leaf_K, hash_func_flush_update);
					write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
				}
			}
		}
		else { // counter+1 �ϸ�־û�
			write_cache(global_ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);
			//�ϸ�־û�global-tree��root
			BMT_path_update(g_global_BMTree, global_leaf_K, hash_func_flush_update);
			write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
		}
	}
	
	//���ﴦ��ӽ��ܵ�����
	g_Statistics.inc_user_write_cntr();
	g_Statistics.inc_mem_write_cycle_cntr();
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	g_Statistics.t_lat_add(memory_write_latency_unit); //��������д�ص�ʱ��
	//���������

	latency_t start_update = g_Statistics.get_t_latency();

	//��̨��⣬�����ɵ��ӳ�Ҫ����
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
	//ProMT-NPд�������
}



