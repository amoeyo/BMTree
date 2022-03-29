#include "promt_np_rw.h"

extern uint8_t* cacheline_register; //64B�����Ĵ���
extern uint8_t* cacheline_register_extra;

page_addr_t g_hotpage_map[PROMT_HOT_REAL_LEAF_N]; //hot tree��page��ӳ�䣬ֱ��ͨ��hot tree index������page��ַ

uint64_t g_valid_leaf_N = 0;

extern ds_addr_t DescriptorBlockIdxTable;

#ifdef PROMT_DEBUG
void print_hotpage_map() {
	for (int i = 0; i < PROMT_HOT_REAL_LEAF_N; i++) {
		if (g_hotpage_map[i]) {
			fmt_print_k_hex_v(i, g_hotpage_map[i]);
		}
	}
}
#endif


//ProMT-NP����
status promt_np_read(phy_addr_t addr, size_t n_bytes, Tick& read_latency) {
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
		status ds_cache_op_status = read_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));
		if (ds_cache_op_status != CACHE_HIT) {
			//δ����д�ز����ӳ٣����������NVMд�����ӵ���bmt_extra_write���棬����Ҫ֮�������Ӽ�����
			latency_t lat = g_Statistics.get_t_latency();
			lat -= memory_write_latency_unit;
			g_Statistics.set_t_latency(lat);
		}

		uint64_t hot_index;
		// ��ȡPadding/Hot EC
		ds_register.get_hot_addr(&hot_index);
		//������������֤
		reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
		if (hot_index) {
			//ͨ��hot tree��֤��page_addrָʾ����hot treeҶ�������õ���ҳ��ַ����ͬʱҲ��region tree��leaf_K
			status stat = BMT_verify_counter(g_hot_BMTree, g_global_BMTree, __leaf_K, hot_index, g_hot_BMTree_root);
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
	g_Statistics.t_lat_add(parallel_memory_read_latency_unit);
	g_Statistics.inc_user_read_cntr();
	//++g_tick_clock
	
	read_latency = g_Statistics.get_t_latency() - read_latency;
#ifdef PROMT_DEBUG
	//print_mq();
#endif
	//ProMT-NP���������
	return SUCCESS;
}


//������MQ�������
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
				ds_register.set_frame_pointer(0);
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
				write_ds_cache((ds_addr_t)DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t), true);
			}
		}
	}
	return SUCCESS;
}

status promt_np_write(phy_addr_t addr, size_t n_bytes, Tick& write_latency) {
	phy_addr_t __addr = (phy_addr_t)addr;
	addr_t ctr_addr = 0; //����Ҫ����__addr�������飬Ȼ�������ƫ�ƣ�������ȫ�����е�Ҷ�ӵ�λ�á�
	uint32_t __leaf_K = 0, __I = 0;

	write_latency = g_Statistics.get_t_latency();

	BMT_get_ctr_attr_global(g_global_BMTree, __addr, &ctr_addr, &__leaf_K, &__I);
	status cache_op_status = read_cache(ctr_addr, cacheline_register_extra, CACHELINE);	//������

	page_addr_t page_addr = __leaf_K;
	ds_addr_t cur_ds = DescriptorBlockIdxTable + page_addr;
	ds_blk_t ds_register; //�Ĵ���
	status ds_cache_op_status = read_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));
#ifdef PROMT_DEBUG
	//ds_register.print();
#endif
	if (ds_cache_op_status != CACHE_HIT) {
		//δ����д�ز����ӳ٣����������NVMд�����ӵ���bmt_extra_write���棬����Ҫ֮�������Ӽ�����
		latency_t lat = g_Statistics.get_t_latency();
		lat -= memory_write_latency_unit;
		g_Statistics.set_t_latency(lat);
	}

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
	ds_register.set_frame_pointer(0);

	reg_data_move(cacheline_register, cacheline_register_extra, CACHELINE);
	if (cache_op_status != CACHE_HIT) {
		if (hot_index) {
			//verify hot tree
			status stat = BMT_verify_counter(g_hot_BMTree, g_global_BMTree, __leaf_K, hot_index, g_hot_BMTree_root);
			if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
				return FAILURE;
			}
		}
		else {
			//verify global tree
			status stat = BMT_verify_counter(g_global_BMTree, __leaf_K, 0, g_global_BMTree_root);
			if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
				return FAILURE;
			}
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

	//��д��ȥ������û���õ��Ĵ��������»����´ӻ����������������ֵ
	write_cache(ctr_addr, &cacheline_register_extra, CACHELINE);

	//�����Ƿ����ж���Ҫ�жϣ�δ��������¶�һ��verify�Ĺ���
	if (hot_index) {
		//counter+1
		// ����hot tree
		BMT_path_update(g_hot_BMTree, g_global_BMTree, __leaf_K, hot_index, g_hot_BMTree_root);
		// ����֮���dsҪ�ǵ�д��ȥ
		write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
	}
	else {
		if (acc_num == 4) {
			//hot treeδ������ʱds�Ѿ�push��Q2��β��
			if (g_valid_leaf_N < PROMT_HOT_REAL_LEAF_N) {
				// ���£����ӣ�ӳ��
				g_hotpage_map[g_valid_leaf_N] = __addr >> PAGE_BITS;
				hot_index = g_valid_leaf_N;
				// ���õ�ǰds��Padding/Hot EC
				ds_register.set_hot_addr(hot_index);
				// ����Hot Tree��Ҷ�ӽڵ��ڵ�ǰ��global tree����
				BMT_path_update(g_hot_BMTree, g_global_BMTree, __leaf_K, hot_index, g_hot_BMTree_root);
				// �����º��dsд�ػ���
				write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
				g_valid_leaf_N++;
			}
			// hot tree���ˣ���Ҫѡ��Q3ͷ�ڵ�����滻
			else {
				// ��ȡQ3�ĵ�һ��page��ַ������������Ӧpage��ds�Ѿ��ӻ����ж����������㲢д���棬Q3ͷָ���޸�
				if (get_queue_idx_size(VICTIM_QUEUE_INDEX)) {
					page_addr_t victim_leaf_K;
					uint64_t victim_hot_index;
					ds_blk_t victim_ds;
					evict_queue_head(VICTIM_QUEUE_INDEX, &victim_ds);

					victim_ds.get_hot_addr(&victim_hot_index);
					victim_ds.get_page_addr(&victim_leaf_K);
					// ��cache�ж���Ӧpage��node��64B���������������֤
					addr_t victim_addr;
					BMT_get_leaf_K_addr(g_global_BMTree, victim_leaf_K, &victim_addr);
					status victim_stat = read_cache(victim_addr, cacheline_register, CACHELINE);
					// δ��������֤hot tree���滻�Ľڵ��ڱ��滻ǰһ����hot tree�ϣ�����Ͷ�һ��
					if (victim_stat != CACHE_HIT) {
						status stat = BMT_verify_counter(g_hot_BMTree, g_global_BMTree, victim_leaf_K, hot_index, g_hot_BMTree_root);
						if (stat != VERIFY_SUCCESS && stat != HASH_ROOT_MORE) {
							return FAILURE;
						}
					}

					//��֤��������global tree���ϸ�־û�9*150�ӳ�
					BMT_path_update(g_global_BMTree, victim_leaf_K, hash_func_flush_update);

					// ��ǰд�����Ӧ��counter���µ�hot tree
					BMT_path_update(g_hot_BMTree, g_global_BMTree, __leaf_K, hot_index, g_hot_BMTree_root);

					//���¶�ӦDS��hot tree�е�λ�ã�����ռ��victim node��λ�ã�����Ҫ��д�ز�����
					g_hotpage_map[victim_hot_index] = __leaf_K;
					ds_register.set_hot_addr(victim_hot_index);
					write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t), true);
				}
				else {
					//����global tree���ϸ���л�9*150�ӳ�
					BMT_path_update(g_global_BMTree, __leaf_K, hash_func_flush_update);
					// ����֮���dsҪ�ǵ�д��ȥ
					write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
				}
			}
		}
		else {
			//����global tree���ϸ���л�9*150�ӳ�
			BMT_path_update(g_global_BMTree, __leaf_K, hash_func_flush_update);
			// ����֮���dsҪ�ǵ�д��ȥ
			write_ds_cache(DescriptorBlockIdxTable + page_addr, &ds_register, sizeof(ds_blk_t));
		}
	}
	

	bool write_back = ((minor % n_counter_write_back_threshold) == 0);
	write_cache(ctr_addr, cacheline_register_extra, CACHELINE, nullptr, write_back);

	//���ﴦ��ӽ��ܵ�����
	g_Statistics.inc_user_write_cntr();
	g_Statistics.inc_mem_write_cycle_cntr();
	g_Statistics.t_lat_add(calculate_otp_latency_unit);
	g_Statistics.t_lat_add(memory_write_latency_unit); //��������д�ص�ʱ��
	//���������

	latency_t start_update = g_Statistics.get_t_latency();

	update_g_multiqueue();

	latency_t update_lat = g_Statistics.get_t_latency() - start_update;

	write_latency = g_Statistics.get_t_latency() - write_latency - update_lat;

#ifdef PROMT_DEBUG
	//print_hotpage_map();
	//print_mq();
#endif
	return SUCCESS;
	//ProMT-NPд�������
}



