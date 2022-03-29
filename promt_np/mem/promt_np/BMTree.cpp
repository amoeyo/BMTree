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

//һЩ����ķ���������Ҫ����hash����
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, hash_func_t hash) {
	//check leaf_K < bmTree.leaf_N
	return  __BMT_traverse_path(bmTree.base, bmTree.leaf_N, leaf_K, bmTree.root,
		bmTree.sizeof_node, hash, bmTree.leaf_N);
}

//hot_tree��Ҫ���øýӿڣ����õ�cacheline_register
//���ⲿ��֤һ������Ϊhot tree��Ҷ�ӽڵ�ʵ��������global tree��
status BMT_path_update(const BMTree& bmTree_hot, const BMTree& bmTree_region,
	uint32_t leaf_K, uint32_t hot_index, addr_t root) {
	// �ҵ�hot tree�ж�Ӧ��λ�ã���ʱcacheline_register����region tree�д�ŵ�leaf������Ҫȡhot tree�еĸ�����֤
	addr_t region_base = bmTree_region.base;
	addr_t cur_root = bmTree_hot.base;

	bmTree_hot.update_hash(&((root_t)cur_root)[hot_index],
		region_base + leaf_K * bmTree_region.sizeof_node, bmTree_hot.sizeof_node);
	//hash(root,cacheline,n)
	// hot_index��hot tree��Ҷ��������ʵ������û�д洢�ռ�ģ�����Ҫ���������֤�ĵڶ���node��λ��
	uint32_t __leaf_root_K;
	__leaf_root_K = next_level_idx(hot_index, g_exp);
	// �����leaf_N = 64���ǵڶ����ˣ�baseҲ�ǵڶ�����׵�ַ
	return __BMT_traverse_path(bmTree_hot.base, bmTree_hot.leaf_N, __leaf_root_K, root,
		bmTree_hot.sizeof_node, bmTree_hot.update_hash, bmTree_hot.leaf_N);

}


//region_tree��back_tree���Ե�������ӿ�,���õ�cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.verify_hash, bmTree.leaf_N);
}

// hot tree��verify֮ǰ��Ҷ�ӽڵ��Ѿ������˼Ĵ�����
// Ŀǰ��֪�����½ڵ���hot tree�е�λ��(hot_index\leaf_K)�����Լ��������Ӧ��һ���ĸ��ڵ�
// �����leaf_K��region tree��leaf�ı�ţ�24λ��ʵ����ҳ��ַֻ��22λ������ֱ����hot 
// ��ʱ��leaf_K������ʵ�����ǵڶ��㣿Ȼ��һֱ������֤
status BMT_verify_counter(const BMTree& bmTree_hot, const BMTree& bmTree_region, uint32_t leaf_K,
	uint32_t hot_index, addr_t root) {
	// �ҵ�hot tree�ж�Ӧ��λ�ã���ʱcacheline_register����region tree�д�ŵ�leaf������Ҫȡhot tree�еĸ�����֤
	addr_t region_base = bmTree_region.base;
	addr_t cur_root = bmTree_hot.base;

	status ret = bmTree_hot.verify_hash(&((root_t)cur_root)[hot_index], region_base + leaf_K * bmTree_region.sizeof_node, bmTree_region.sizeof_node);
	//hash(root,cacheline,n)
	if (ret == HASH_ROOT_MORE) {
		// hot_index��hot tree��Ҷ��������ʵ������û�д洢�ռ�ģ�����Ҫ���������֤�ĵڶ���node��λ��
		uint32_t __leaf_root_K;
		__leaf_root_K = next_level_idx(hot_index, g_exp);
		// �����leaf_N = 64���ǵڶ����ˣ�baseҲ�ǵڶ�����׵�ַ
		return __BMT_traverse_path(bmTree_hot.base, bmTree_hot.leaf_N, __leaf_root_K, root,
			bmTree_hot.sizeof_node, bmTree_hot.verify_hash, bmTree_hot.leaf_N);
	}
	else return ret;

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


//����globalBMT��һ�����������й̶��������������ﵥ��дһ����CTR�����ĺ���
status BMT_get_ctr_attr_global(BMTree& bmTree, phy_addr_t addr,
	addr_t* ctr, uint32_t* leaf_K, uint32_t* I) {
	//phy_addr_t high_bits = addr & (~MEMORY_MASK);
	//check high_bits == 0;
	//*BMT_K = (uint32_t)((addr >> SEGMENT_BITS) & BMT_K_MASK);
	*leaf_K = (uint32_t)((addr >> PAGE_BITS) & PROMT_GLOBAL_LEAF_MASK);
	*I = (uint32_t)((addr >> CACHELINE_BITS) & PROMT_CTR_INDEX_MASK);
	*ctr = bmTree.base + bmTree.sizeof_node * (*leaf_K);
	return SUCCESS;
}

status BMT_get_leaf_K_addr(BMTree& bmTree, uint32_t leak_K, addr_t* addr) {
	*addr = bmTree.base + bmTree.sizeof_node * leak_K;
	return SUCCESS;
}


BMTree&  BMTreeMgr::global_BMTree() { return __global_BMTree; }
BMTree&  BMTreeMgr::hot_BMTree() { return __hot_BMTree; }

BMTreeMgr::BMTreeMgr() {
	//global tree�� 9��
	__global_BMTree.BMT_N = 1;
	__global_BMTree.leaf_N = PROMT_GLOBAL_LEAF_N; //1<<24
	__global_BMTree.root = memory_alloc_cacheline();
	__global_BMTree.sizeof_node = SIZE_OF_NODE;
	BMT_alloc(__global_BMTree);
	//BMT_construct(__global_BMTree);

	//hot tree��4�㣬����ʵ�ʴ洢�ռ�ֻ��3��
	__hot_BMTree.BMT_N = 1;
	__hot_BMTree.leaf_N = PROMT_HOT_LEAF_N;
	__hot_BMTree.root = memory_alloc_cacheline();
	__hot_BMTree.sizeof_node = SIZE_OF_NODE;
	BMT_alloc(__hot_BMTree);
	// ��ʼ��ʱhot treeʵ�����ǿյ�
	//BMT_construct(__hot_BMTree);
}

BMTreeMgr& g_BMTreeMgr = BMTreeMgr::get_instance();

BMTree g_global_BMTree = g_BMTreeMgr.global_BMTree();
addr_t g_global_BMTree_root = g_global_BMTree.root;
BMTree g_hot_BMTree = g_BMTreeMgr.hot_BMTree();
addr_t g_hot_BMTree_root = g_hot_BMTree.root;


