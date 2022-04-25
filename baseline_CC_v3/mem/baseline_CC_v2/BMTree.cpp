#include "BMTree.h"

extern uint8_t* cacheline_register; //64B�����Ĵ���
extern uint8_t* cacheline_register_extra;

//�����root�ǼĴ���
status hash_func_UT(hash_t* root, cacheline_t cacheline, uint32_t n) {
	//check n <= 64
	//�����root�Ǹ��Ĵ���
	if (root == 0) return FAILURE;
	g_Statistics.hash_lat_add(hash_latency_unit); //����ʱ��
	g_Statistics.inc_hash_cntr();
	g_Statistics.t_lat_add(hash_latency_unit);
	hash_t __root = 0;
	hash_t* p = (hash_t*)cacheline;
	uint32_t cnt = n / (sizeof(hash_t) / sizeof(*cacheline)); //sizeof����ʱ����
	for (size_t i = 0; i < cnt; i++) {
		__root += *(p++);
	}
	*root = __root;
	return SUCCESS;
}

hash_func_t g_hash = hash_func_UT;

//������cachelineһ�㶼��CACHELINE�����SIZE_OF_NODE == CACHELINE
//��root��ͨ������cacheline����
status hash_func_verify(hash_t* root, cacheline_t cacheline, uint32_t n) {
	//������ݲ��ȣ���֤ʧ��
	//���������ȣ���
	//root��cache�����У�����֤�ɹ���
	//root��cache�в����У��������֤��

	hash_t __root = 0;//�Ĵ���
	hash_t real_root = 0;
	status cache_hit = CACHE_MISS;

	read_cache(cacheline, cacheline_register, n);
	g_hash(&__root, cacheline_register, n);
	cache_hit = read_cache(root, &real_root, sizeof(real_root));

	if (__root == real_root) {
		return (cache_hit == CACHE_HIT) ? VERIFY_SUCCESS : HASH_ROOT_MORE;
	}
	else {
		return VERIFY_FAILURE;
	}
}

// ��cacheline_register����������ϣֵ
status hash_func_verify_using_large_reg(hash_t* root, cacheline_t cacheline, uint32_t n) {
	//������ݲ��ȣ���֤ʧ��
	//���������ȣ���
	//root��cache�����У�����֤�ɹ���
	//root��cache�в����У��������֤��

	hash_t __root = 0;//�Ĵ���
	hash_t real_root = 0;
	status cache_hit = CACHE_MISS;

	g_hash(&__root, cacheline_register, n);

	phy_addr_t root_addr = (phy_addr_t)root;
	phy_addr_t __root_addr = __make_addr_cacheline_align(root_addr);
	phy_addr_t __byte_offset = __parse_bytes_idx(root_addr);
	//check __byte_offset = K * 8
	cache_hit = read_cache((addr_t)__root_addr, cacheline_register, CACHELINE); //��һ��Ҫ�õĶ�����
	real_root = *((hash_t*)(cacheline_register + __byte_offset));

 	if (__root == real_root) {
		return (cache_hit == CACHE_HIT) ? VERIFY_SUCCESS : HASH_ROOT_MORE;
	}
	else {
		return VERIFY_FAILURE;
	}
}

///dr_treeʹ��
//����һ��д��������ֻ������д�뻺���С�
//�����root�������Ǹ��ڴ��ַ�����������ȫ�ָ��������Էŵ�������һ������ط�����֤ÿ�ζ�����
//�ú��������꣬���Խ�ȫ�ָ���ֵ����д���Ĵ���
status hash_func_update(hash_t* root, cacheline_t cacheline, uint32_t n) {
	//bulk_update��Ӧ������������������ϲ�Ϳ������
	//������LARGE_REGISTER_LINK_OPTIģʽ����Ϊupdate�������ܻ��Ӻ󣬻���������ʹ�Ĵ����ڲ�һ��������ֵ��

	hash_t __root = 0; //�Ĵ���
	read_cache(cacheline, cacheline_register, n);
	g_hash(&__root, cacheline_register, n);
	write_cache(root, &__root, sizeof(__root));

	return HASH_ROOT_MORE;
}

///baselineʹ��,���Ը���ʱ��
status hash_func_background_update(hash_t* root, cacheline_t cacheline, uint32_t n) {
	hash_t __root = 0; //�Ĵ���
	latency_t start_point = g_Statistics.get_t_latency();
	status read_op_stat = read_cache(cacheline, cacheline_register, n);
	g_hash(&__root, cacheline_register, n);
	write_cache(root, &__root, sizeof(__root));
	g_Statistics.set_t_latency(start_point);//��̨���²�����ʱ��
	return SUCCESS;
}

//strict persistʹ�ã����ظ���ʱ��
status hash_func_flush_update(hash_t* root, cacheline_t cacheline, uint32_t n) {
	hash_t __root = 0; //�Ĵ���
	read_cache(cacheline, cacheline_register, n);
	g_hash(&__root, cacheline_register, n);
	//��ȥhash��֤�����ʱ�ӣ�����ֻ��������ʱ�ӣ�hash���ۼ�ʱ��û�й�
	//���µ��ӳٱ��־û����أ����԰�hash��80����
	latency_t add_latency = g_Statistics.get_t_latency();
	g_Statistics.set_t_latency(add_latency - hash_latency_unit);
	write_cache(root, &__root, sizeof(__root), nullptr, true);//д��+150
	return HASH_ROOT_MORE;
}

//Atomic Updateʹ��
//���acc table��ˢ��acc table�м�¼�Ļ�����
status hash_func_check_acc_update(hash_t* root, cacheline_t cacheline, uint32_t n) {
	hash_t __root = 0; //�Ĵ���
	read_cache(cacheline, cacheline_register, n, nullptr, true);
	g_hash(&__root, cacheline_register, n);
	//����acc_table
	write_cache(root, &__root, sizeof(__root), nullptr, false, true);
	return HASH_ROOT_MORE;
}


#ifdef BMT_PROJECT_DEBUG
//�����cacheline���ڴ��ַ����hash_func_update,hash_func_verify����ͬ��
status hash_identify_print(hash_t* res, cacheline_t cacheline, uint32_t n) {
	read_cache(cacheline, cacheline_register, n);
	hash_t* p = (hash_t*)cacheline_register;
	uint32_t cnt = n / (sizeof(hash_t) / sizeof(*cacheline));
	for (size_t i = 0; i < cnt; i++) {
		std::cout << *p++ << " ";
	}
	return SUCCESS;
}
#endif // BMT_PROJECT_DEBUG

uint32_t BMT_get_tree_node_N(uint32_t leaf_N) {
	uint32_t cur_leaf_N = leaf_N;
	uint32_t total_leaf_N = 0;
	do {
		total_leaf_N += cur_leaf_N;
		cur_leaf_N = next_level_N(cur_leaf_N, g_exp);
	} while (cur_leaf_N > 1); //���������
	return total_leaf_N;
}

//�����������
status __BMT_alloc(addr_t* base, uint32_t* BMT_size, uint32_t leaf_N,
	uint32_t sizeof_node, uint32_t BMT_N) {
	uint32_t tree_node_N = BMT_get_tree_node_N(leaf_N);
	*BMT_size = tree_node_N * sizeof_node;
	uint32_t tree_region_size = *BMT_size * BMT_N;
	*base = memory_alloc(tree_region_size); //���Զ��ÿգ����ܷ���ʧ��
	return (*base) ? SUCCESS : FAILURE; // status 0 for success 
}

status __BMT_free(addr_t base) {
	memory_free(base);
	return SUCCESS;
}

status BMT_alloc(BMTree& bmTree) {
	return __BMT_alloc(&bmTree.base, &bmTree.sizeof_BMT, bmTree.leaf_N, bmTree.sizeof_node, bmTree.BMT_N);
}

status BMT_free(BMTree& bmTree) {
	return __BMT_free(bmTree.base);
}

status __BMT_traverse_tree(addr_t base, uint32_t leaf_N, addr_t root,
	uint32_t sizeof_node, hash_func_t hash, uint32_t valid_leaf_N) {
	uint32_t cur_level_node_N = leaf_N;
	uint32_t cur_level_valid_node_N = min_xy(leaf_N, valid_leaf_N);
	addr_t cur_base = base;
	
	addr_t cur_root = base + cur_level_node_N * sizeof_node;
	while (cur_level_valid_node_N > 1) {
		uint32_t next_level_node_N = next_level_N(cur_level_node_N, g_exp);
		uint32_t next_level_valid_node_N = next_level_N(cur_level_valid_node_N, g_exp);
		if (next_level_valid_node_N <= 1) {
			cur_root = root;
		}
#ifdef BMT_PROJECT_DEBUG
		if (hash == hash_identify_print) { fmt_print_sep(level_sep); }
#endif // FMT_DEBUG
		for (size_t i = 0; i < cur_level_valid_node_N; i++) {
			//������ʱ����ֵ�̶����Ͳ������⴦���ˡ��ú��������Ǳ��������������жϡ�
			//��ַ�������ֱ���㣬��Ҫ��ȡ��ַ������֪��������զ���������ᷢ����ķô��?
			//��Щ�˷���ʵ����������λʵ��
			hash(&((root_t)cur_root)[i], cur_base + i * sizeof_node, sizeof_node);
		}
#ifdef BMT_PROJECT_DEBUG
		if (hash == hash_identify_print) { fmt_print_sep(level_sep); }
#endif // FMT_DEBUG

		cur_level_node_N = next_level_node_N;
		cur_level_valid_node_N = next_level_valid_node_N;
		cur_base = cur_root;
		cur_root += cur_level_node_N * sizeof_node;
	}
#ifdef BMT_PROJECT_DEBUG
	hash(0, root, sizeof_node); //�����������Ժ������ã���������ֱ�ӷ���,�رյ����򲻻�����ʱ��
#endif // FMT_DEBUG
	return SUCCESS;
}

status __BMT_traverse_tree(addr_t base, uint32_t leaf_N, addr_t root,
	uint32_t sizeof_node, uint32_t BMT_N, uint32_t sizeof_BMT, hash_func_t hash, uint32_t valid_leaf_N) {
	for (size_t i = 0; i < BMT_N; i++) { //��ʼ������Ҳ����
#ifdef BMT_PROJECT_DEBUG
		if (hash == hash_identify_print) { fmt_print_sep(tree_sep); }
#endif // FMT_DEBUG
		__BMT_traverse_tree(base + i * sizeof_BMT, leaf_N,
			root + i * sizeof_node, sizeof_node, hash, valid_leaf_N);
#ifdef BMT_PROJECT_DEBUG
		if (hash == hash_identify_print) { fmt_print_sep(tree_sep); }
#endif // FMT_DEBUG
	}
	return SUCCESS;
}

//������ܻ��кܶ�������Ҫ����
status __BMT_traverse_path(addr_t base, uint32_t leaf_N, uint32_t leaf_K, addr_t root,
	uint32_t sizeof_node, hash_func_t hash, uint32_t valid_leaf_N) {
	uint32_t cur_node_K = leaf_K;
	uint32_t cur_level_node_N = leaf_N;
	uint32_t cur_level_valid_node_N = min_xy(leaf_N, valid_leaf_N);
	addr_t cur_base = base;
	addr_t cur_root = base + cur_level_node_N * sizeof_node;
	status ret = HASH_ROOT_MORE;
	while (cur_level_valid_node_N > 1) {
		uint32_t next_level_node_N = next_level_N(cur_level_node_N, g_exp);
		uint32_t next_level_valid_node_N = next_level_N(cur_level_valid_node_N, g_exp);
		if (next_level_valid_node_N <= 1) {
			cur_root = root;
		}

		ret = hash(&((root_t)cur_root)[cur_node_K], cur_base + cur_node_K * sizeof_node, sizeof_node);
		if (ret != HASH_ROOT_MORE) { return ret; }
		cur_node_K = next_level_idx(cur_node_K, g_exp);
		cur_level_node_N = next_level_node_N;
		cur_level_valid_node_N = next_level_valid_node_N;
		cur_base = cur_root;
		cur_root += cur_level_node_N * sizeof_node;
	}
	return ret;
}

status BMT_construct(BMTree& bmTree) {
	return __BMT_traverse_tree(bmTree.base, bmTree.leaf_N, bmTree.root,
		bmTree.sizeof_node, bmTree.BMT_N, bmTree.sizeof_BMT, bmTree.update_hash, bmTree.leaf_N);
}

status BMT_construct(BMTree& bmTree, uint32_t valid_leaf_N) {
	return __BMT_traverse_tree(bmTree.base, bmTree.leaf_N, bmTree.root,
		bmTree.sizeof_node, bmTree.BMT_N, bmTree.sizeof_BMT, bmTree.update_hash, valid_leaf_N);
}

//�����root˳���Ӧ��,back_tree���Ե�������ӿ�
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K) {
	//check BMT_K < bmTree.BMT_N
	//check leaf_K < bmTree.leaf_N
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	addr_t root = bmTree.root + bmTree.sizeof_node * BMT_K;
	return  __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.update_hash, bmTree.leaf_N);
}

//һЩ����ķ���������Ҫ����hash����
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, hash_func_t hash) {
	//check leaf_K < bmTree.leaf_N
	return  __BMT_traverse_path(bmTree.base, bmTree.leaf_N, leaf_K, bmTree.root,
		bmTree.sizeof_node, hash, bmTree.leaf_N);
}

//region_tree��back_tree���Ե�������ӿ�,���õ�cacheline_register
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.update_hash, bmTree.leaf_N);
}

//fore_tree��Ҫ���øýӿڡ����õ�cacheline_register
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K,
	addr_t root, uint32_t valid_leaf_N) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.update_hash, valid_leaf_N);
}

//verify�Ĺ��̣�����counter�Ƿ��ڻ����У���������֤�������ȶ���cache�У�
//���������cacheline�Ĵ������ٵ��øú������Ա㲻���ٴη���ô档
//back_tree���Ե�������ӿ�
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	addr_t root = bmTree.root + bmTree.sizeof_node * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.verify_hash, bmTree.leaf_N);
}

//region_tree��back_tree���Ե�������ӿ�,���õ�cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.verify_hash, bmTree.leaf_N);
}

//fore_tree��Ҫ���øýӿڡ�cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K,
	addr_t root, uint32_t valid_leaf_N) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.verify_hash, valid_leaf_N);
}

status __BMT_calculate_root(addr_t base, uint32_t leaf_N, uint32_t sizeof_node,
	uint32_t valid_leaf_N, addr_t* root) {
	uint32_t cur_level_node_N = leaf_N;
	uint32_t cur_level_valid_node_N = min_xy(leaf_N, valid_leaf_N);
	//addr_t cur_base = base;
	addr_t cur_root = base + cur_level_node_N * sizeof_node;
	while (cur_level_valid_node_N > 1) {
		uint32_t next_level_node_N = next_level_N(cur_level_node_N, g_exp);
		uint32_t next_level_valid_node_N = next_level_N(cur_level_valid_node_N, g_exp);
		if (next_level_valid_node_N <= 1) {
			break;
		}

		cur_level_node_N = next_level_node_N;
		cur_level_valid_node_N = next_level_valid_node_N;
		//cur_base = cur_root;
		cur_root += cur_level_node_N * sizeof_node;
	}
	*root = cur_root;
	return SUCCESS;
}

status BMT_get_root_for_valid_leaf_N(BMTree& bmTree,
	uint32_t valid_leaf_N, addr_t* root) {
	return __BMT_calculate_root(bmTree.base, bmTree.leaf_N,
		bmTree.sizeof_node, valid_leaf_N, root);
}

status BMT_get_ctr_attr(BMTree& bmTree, phy_addr_t addr,
	addr_t* ctr, uint32_t* BMT_K, uint32_t* leaf_K, uint32_t* I) {
	//phy_addr_t high_bits = addr & (~MEMORY_MASK);
	//check high_bits == 0;

	*BMT_K = (uint32_t)((addr >> SEGMENT_BITS) & BMT_K_MASK);
	*leaf_K = (uint32_t)((addr >> PAGE_BITS) & LEAF_MASK);
	*I = (uint32_t)((addr >> CACHELINE_BITS) & CTR_INDEX_MASK);
	*ctr = bmTree.base + bmTree.sizeof_BMT * (*BMT_K) + bmTree.sizeof_node * (*leaf_K);
	return SUCCESS;
}

//����baselineBMT��һ�����������й̶��������������ﵥ��дһ����CTR�����ĺ���
status BMT_get_ctr_attr_baseline(BMTree& bmTree, phy_addr_t addr,
	addr_t* ctr, uint32_t* leaf_K, uint32_t* I) {
	//phy_addr_t high_bits = addr & (~MEMORY_MASK);
	//check high_bits == 0;

	//*BMT_K = (uint32_t)((addr >> SEGMENT_BITS) & BMT_K_MASK);
	*leaf_K = (uint32_t)(((addr >> (BASELINE_CTR_INDEX_BITS + CACHELINE_BITS)) & BASELINE_LEAF_MASK));
	*I = (uint32_t)((addr >> CACHELINE_BITS) & BASELINE_CTR_INDEX_MASK);
	*ctr = bmTree.base + bmTree.sizeof_node * (*leaf_K);
	return SUCCESS;
}

status BMT_get_leaf_K_addr(BMTree& bmTree, uint32_t leak_K, addr_t* addr) {
	*addr = bmTree.base + bmTree.sizeof_node * leak_K;
	return SUCCESS;
}

//���Ժ���
#ifdef BMT_PROJECT_DEBUG
status BMT_set_K_tree_K_leaf_I_QWORD(BMTree& bmTree, uint32_t BMT_K, uint32_t leaf_K,
	uint32_t I, uint64_t value) {
	addr_t __addr = bmTree.base + bmTree.sizeof_BMT * BMT_K +
		leaf_K * bmTree.sizeof_node + I * sizeof(value);
	write_cache(__addr, &value, sizeof(value), true);
	return SUCCESS;
}


void __BMT_fmt_print(const BMTree& bmTree) {
	fmt_print_kv("bmTree.base", (void*)bmTree.base);
	fmt_print_kv("bmTree.root", (void*)bmTree.root);
	fmt_print_kv("bmTree.leaf_N", bmTree.leaf_N);
	fmt_print_kv("bmTree.sizeof_node", bmTree.sizeof_node);
	fmt_print_kv("bmTree.BMT_N", bmTree.BMT_N);
	fmt_print_kv("bmTree.sizeof_BMT", bmTree.sizeof_BMT);
}

status BMT_fmt_print(const BMTree& bmTree) {
	__BMT_fmt_print(bmTree);
	return __BMT_traverse_tree(bmTree.base, bmTree.leaf_N, bmTree.root,
		bmTree.sizeof_node, bmTree.BMT_N, bmTree.sizeof_BMT, hash_identify_print, bmTree.leaf_N);
}
#else
status BMT_set_K_tree_K_leaf_I_QWORD(BMTree& bmTree, uint32_t BMT_K, uint32_t leaf_K,
	uint32_t I, uint64_t value) {
	return SUCCESS;
}
void __BMT_fmt_print(const BMTree& bmTree) { }
status BMT_fmt_print(const BMTree& bmTree) { return SUCCESS; }

#endif // BMT_PROJECT_DEBUG



BMTree BMTreeMgr::baseline_BMTree() { return __baseline_BMTree; }

BMTreeMgr::BMTreeMgr() {
	__baseline_BMTree.BMT_N = 1;
	__baseline_BMTree.leaf_N = BASELINE_LEAF_N;
	__baseline_BMTree.root = memory_alloc_cacheline();
	__baseline_BMTree.sizeof_node = SIZE_OF_NODE;
	BMT_alloc(__baseline_BMTree);
	//BMT_construct(__baseline_BMTree);
}

BMTreeMgr& g_BMTreeMgr = BMTreeMgr::get_instance();

BMTree g_baseline_BMTree = g_BMTreeMgr.baseline_BMTree();
addr_t g_baseline_BMTree_root = g_baseline_BMTree.root;


