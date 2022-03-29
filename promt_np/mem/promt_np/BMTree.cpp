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

//一些特殊的方案可能需要灵活的hash函数
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, hash_func_t hash) {
	//check leaf_K < bmTree.leaf_N
	return  __BMT_traverse_path(bmTree.base, bmTree.leaf_N, leaf_K, bmTree.root,
		bmTree.sizeof_node, hash, bmTree.leaf_N);
}

//hot_tree需要调用该接口，会用到cacheline_register
//在外部验证一次是因为hot tree的叶子节点实际上是在global tree中
status BMT_path_update(const BMTree& bmTree_hot, const BMTree& bmTree_region,
	uint32_t leaf_K, uint32_t hot_index, addr_t root) {
	// 找到hot tree中对应的位置，此时cacheline_register中是region tree中存放的leaf，这里要取hot tree中的根做验证
	addr_t region_base = bmTree_region.base;
	addr_t cur_root = bmTree_hot.base;

	bmTree_hot.update_hash(&((root_t)cur_root)[hot_index],
		region_base + leaf_K * bmTree_region.sizeof_node, bmTree_hot.sizeof_node);
	//hash(root,cacheline,n)
	// hot_index是hot tree的叶子索引，实际上是没有存储空间的，所以要先算出待验证的第二层node的位置
	uint32_t __leaf_root_K;
	__leaf_root_K = next_level_idx(hot_index, g_exp);
	// 这里的leaf_N = 64，是第二层了，base也是第二层的首地址
	return __BMT_traverse_path(bmTree_hot.base, bmTree_hot.leaf_N, __leaf_root_K, root,
		bmTree_hot.sizeof_node, bmTree_hot.update_hash, bmTree_hot.leaf_N);

}


//region_tree和back_tree可以调用这个接口,会用到cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root) {
	addr_t base = bmTree.base + bmTree.sizeof_BMT * BMT_K;
	return __BMT_traverse_path(base, bmTree.leaf_N, leaf_K, root,
		bmTree.sizeof_node, bmTree.verify_hash, bmTree.leaf_N);
}

// hot tree的verify之前，叶子节点已经读到了寄存器中
// 目前已知：更新节点在hot tree中的位置(hot_index\leaf_K)，可以计算出它对应上一层哪个节点
// 这里的leaf_K是region tree上leaf的编号，24位，实际上页地址只有22位，可以直接用hot 
// 此时的leaf_K等数据实际上是第二层？然后一直往上验证
status BMT_verify_counter(const BMTree& bmTree_hot, const BMTree& bmTree_region, uint32_t leaf_K,
	uint32_t hot_index, addr_t root) {
	// 找到hot tree中对应的位置，此时cacheline_register中是region tree中存放的leaf，这里要取hot tree中的根做验证
	addr_t region_base = bmTree_region.base;
	addr_t cur_root = bmTree_hot.base;

	status ret = bmTree_hot.verify_hash(&((root_t)cur_root)[hot_index], region_base + leaf_K * bmTree_region.sizeof_node, bmTree_region.sizeof_node);
	//hash(root,cacheline,n)
	if (ret == HASH_ROOT_MORE) {
		// hot_index是hot tree的叶子索引，实际上是没有存储空间的，所以要先算出待验证的第二层node的位置
		uint32_t __leaf_root_K;
		__leaf_root_K = next_level_idx(hot_index, g_exp);
		// 这里的leaf_N = 64，是第二层了，base也是第二层的首地址
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


//由于globalBMT是一整棵树并且有固定层数，所以这里单独写一个找CTR索引的函数
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
	//global tree， 9层
	__global_BMTree.BMT_N = 1;
	__global_BMTree.leaf_N = PROMT_GLOBAL_LEAF_N; //1<<24
	__global_BMTree.root = memory_alloc_cacheline();
	__global_BMTree.sizeof_node = SIZE_OF_NODE;
	BMT_alloc(__global_BMTree);
	//BMT_construct(__global_BMTree);

	//hot tree，4层，但是实际存储空间只有3层
	__hot_BMTree.BMT_N = 1;
	__hot_BMTree.leaf_N = PROMT_HOT_LEAF_N;
	__hot_BMTree.root = memory_alloc_cacheline();
	__hot_BMTree.sizeof_node = SIZE_OF_NODE;
	BMT_alloc(__hot_BMTree);
	// 初始化时hot tree实际上是空的
	//BMT_construct(__hot_BMTree);
}

BMTreeMgr& g_BMTreeMgr = BMTreeMgr::get_instance();

BMTree g_global_BMTree = g_BMTreeMgr.global_BMTree();
addr_t g_global_BMTree_root = g_global_BMTree.root;
BMTree g_hot_BMTree = g_BMTreeMgr.hot_BMTree();
addr_t g_hot_BMTree_root = g_hot_BMTree.root;


