#include "BMTree.h"

extern uint8_t* cacheline_register; //64B共享大寄存器
extern uint8_t* cacheline_register_extra;

//这里的root是寄存器
status hash_func_UT(hash_t* root, cacheline_t cacheline, uint32_t n) {
	//check n <= 64
	//这里的root是个寄存器
	if (root == 0) return FAILURE;
	g_Statistics.hash_lat_add(hash_latency_unit); //引入时延
	g_Statistics.inc_hash_cntr();
	g_Statistics.t_lat_add(hash_latency_unit);
	hash_t __root = 0;
	hash_t* p = (hash_t*)cacheline;
	uint32_t cnt = n / (sizeof(hash_t) / sizeof(*cacheline)); //sizeof编译时常量
	for (size_t i = 0; i < cnt; i++) {
		__root += *(p++);
	}
	*root = __root;
	return SUCCESS;
}

hash_func_t g_hash = hash_func_UT;

//像这种cacheline一般都是CACHELINE对齐的SIZE_OF_NODE == CACHELINE
//而root则通常不是cacheline对齐
status hash_func_verify(hash_t* root, cacheline_t cacheline, uint32_t n) {
	//如果内容不等，验证失败
	//如果内容相等，则：
	//root在cache中命中，则验证成功。
	//root在cache中不命中，则继续验证根

	hash_t __root = 0;//寄存器
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

// 对cacheline_register里面的数算哈希值
status hash_func_verify_using_large_reg(hash_t* root, cacheline_t cacheline, uint32_t n) {
	//如果内容不等，验证失败
	//如果内容相等，则：
	//root在cache中命中，则验证成功。
	//root在cache中不命中，则继续验证根

	hash_t __root = 0;//寄存器
	hash_t real_root = 0;
	status cache_hit = CACHE_MISS;

	g_hash(&__root, cacheline_register, n);

	phy_addr_t root_addr = (phy_addr_t)root;
	phy_addr_t __root_addr = __make_addr_cacheline_align(root_addr);
	phy_addr_t __byte_offset = __parse_bytes_idx(root_addr);
	//check __byte_offset = K * 8
	cache_hit = read_cache((addr_t)__root_addr, cacheline_register, CACHELINE); //下一个要用的读上来
	real_root = *((hash_t*)(cacheline_register + __byte_offset));

 	if (__root == real_root) {
		return (cache_hit == CACHE_HIT) ? VERIFY_SUCCESS : HASH_ROOT_MORE;
	}
	else {
		return VERIFY_FAILURE;
	}
}

///dr_tree使用
//还有一种写操作，是只将数据写入缓存中。
//这里的root基本上是个内存地址，除非特殊的全局根，根可以放到缓存中一个特殊地方，保证每次都命中
//该函数运行完，可以将全局根的值即刻写到寄存器
status hash_func_update(hash_t* root, cacheline_t cacheline, uint32_t n) {
	//bulk_update不应该在这个函数做。在上层就可以完成
	//不能用LARGE_REGISTER_LINK_OPTI模式，因为update操作可能会延后，或者驱逐，致使寄存器内不一定是最新值。

	hash_t __root = 0; //寄存器
	read_cache(cacheline, cacheline_register, n);
	g_hash(&__root, cacheline_register, n);
	write_cache(root, &__root, sizeof(__root));

	return HASH_ROOT_MORE;
}

///baseline使用,忽略更新时间
status hash_func_background_update(hash_t* root, cacheline_t cacheline, uint32_t n) {
	hash_t __root = 0; //寄存器
	latency_t start_point = g_Statistics.get_t_latency();
	status read_op_stat = read_cache(cacheline, cacheline_register, n);
	g_hash(&__root, cacheline_register, n);
	write_cache(root, &__root, sizeof(__root));
	g_Statistics.set_t_latency(start_point);//后台更新不引入时延
	return SUCCESS;
}

//strict persist使用，隐藏更新时延
status hash_func_flush_update(hash_t* root, cacheline_t cacheline, uint32_t n) {
	hash_t __root = 0; //寄存器
	read_cache(cacheline, cacheline_register, n);
	g_hash(&__root, cacheline_register, n);
	//减去hash验证引入的时延，这里只处理了总时延，hash的累计时延没有管
	//更新的延迟被持久化隐藏，所以把hash的80减掉
	latency_t add_latency = g_Statistics.get_t_latency();
	g_Statistics.set_t_latency(add_latency - hash_latency_unit);
	write_cache(root, &__root, sizeof(__root), nullptr, true);//写回+150
	return HASH_ROOT_MORE;
}

//Atomic Update使用
//检查acc table，刷回acc table中记录的缓存行
status hash_func_check_acc_update(hash_t* root, cacheline_t cacheline, uint32_t n) {
	hash_t __root = 0; //寄存器
	read_cache(cacheline, cacheline_register, n, nullptr, true);
	g_hash(&__root, cacheline_register, n);
	//更新acc_table
	write_cache(root, &__root, sizeof(__root), nullptr, false, true);
	return HASH_ROOT_MORE;
}


#ifdef BMT_PROJECT_DEBUG
//这里的cacheline是内存地址，和hash_func_update,hash_func_verify函数同级
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
	} while (cur_leaf_N > 1); //根单独存放
	return total_leaf_N;
}

//两个输出参数
status __BMT_alloc(addr_t* base, uint32_t* BMT_size, uint32_t leaf_N,
	uint32_t sizeof_node, uint32_t BMT_N) {
	uint32_t tree_node_N = BMT_get_tree_node_N(leaf_N);
	*BMT_size = tree_node_N * sizeof_node;
	uint32_t tree_region_size = *BMT_size * BMT_N;
	*base = memory_alloc(tree_region_size); //会自动置空，可能分配失败
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
			//由于暂时返回值固定，就不做特殊处理了。该函数本意是遍历整棵树，不中断。
			//地址计算最好直接算，不要用取地址符，不知道编译器咋处理，按理不会发生真的访存吧?
			//这些乘法其实都可以用移位实现
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
	hash(0, root, sizeof_node); //遍历根，调试函数会用，其他函数直接返回,关闭调试则不会引入时延
#endif // FMT_DEBUG
	return SUCCESS;
}

status __BMT_traverse_tree(addr_t base, uint32_t leaf_N, addr_t root,
	uint32_t sizeof_node, uint32_t BMT_N, uint32_t sizeof_BMT, hash_func_t hash, uint32_t valid_leaf_N) {
	for (size_t i = 0; i < BMT_N; i++) { //初始化不做也可以
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

//后面可能还有很多其他的要处理
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

//这个是root顺序对应的,back_tree可以调用这个接口
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K) {
	//check BMT_K < bmTree.BMT_N
	//check leaf_K < bmTree.leaf_N
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	addr_t root = bmTree.root + bmTree.sizeof_node * BMT_K;
	return  __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.update_hash, bmTree.leaf_N);
}

//一些特殊的方案可能需要灵活的hash函数
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, hash_func_t hash) {
	//check leaf_K < bmTree.leaf_N
	return  __BMT_traverse_path(bmTree.base, bmTree.leaf_N, leaf_K, bmTree.root,
		bmTree.sizeof_node, hash, bmTree.leaf_N);
}

//region_tree和back_tree可以调用这个接口,会用到cacheline_register
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.update_hash, bmTree.leaf_N);
}

//fore_tree需要调用该接口。会用到cacheline_register
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K,
	addr_t root, uint32_t valid_leaf_N) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.update_hash, valid_leaf_N);
}

//verify的过程，查找counter是否在缓存中，在则不用验证，否则先读到cache中，
//或者特殊的cacheline寄存器，再调用该函数，以便不用再次发起访存。
//back_tree可以调用这个接口
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	addr_t root = bmTree.root + bmTree.sizeof_node * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.verify_hash, bmTree.leaf_N);
}

//region_tree和back_tree可以调用这个接口,会用到cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.verify_hash, bmTree.leaf_N);
}

//fore_tree需要调用该接口。cacheline_register
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

//由于baselineBMT是一整棵树并且有固定层数，所以这里单独写一个找CTR索引的函数
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

//调试函数
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


