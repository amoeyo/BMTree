#ifndef __BASELINE_BMTREE_HH__
#define __BASELINE_BMTREE_HH__

#include "common.h"
#include "metaMgr.h"
#include "unionCtr.h"
#include <stdint.h>
#include <functional>
#include <string>
//遍历的时候寄存器独占,且root不能跨cacheline
//遍历树的路径时，root数据已经在访问叶子时加载完毕。
//#define LARGE_REGISTER_LINK_OPTI

//最后再将声明和实现分开

using hash_t = uint64_t;
using tree_node_t = uint64_t*;
using root_t = tree_node_t;

constexpr uint32_t SIZE_OF_NODE = CACHELINE;
constexpr uint32_t CACHELINES_PER_CTR = PAGE / CACHELINE; // (1 << CTR_INDEX_BITS)
constexpr uint64_t SEG_CTR_NUM = SEGMENT / PAGE; // (1 << LEAF_INDEX_BITS)
constexpr uint64_t MEMORY_MASK = MAKE_MASK(MEMORY_BITS, uint64_t);
constexpr uint64_t BMT_K_MASK = MAKE_MASK(MEMORY_BITS - SEGMENT_BITS, uint64_t);
constexpr uint64_t LEAF_MASK = MAKE_MASK(SEGMENT_BITS - PAGE_BITS, uint64_t);
constexpr uint64_t CTR_INDEX_MASK = MAKE_MASK(PAGE_BITS - CACHELINE_BITS, uint64_t);
constexpr uint64_t N_HASH_PER_NODE = SIZE_OF_NODE / sizeof(hash_t);

//对比方案管固定大小的内存，9层BMTree叶子节点有8^8个64B
constexpr uint64_t BASELINE_BMT_LEVEL = 9;
constexpr uint64_t BASELINE_LEAF_INDEX_BITS = 24;
constexpr uint64_t BASELINE_CTR_INDEX_BITS = 6;//一个cacheline管64个计数器
constexpr uint64_t BASELINE_LEAF_N = 1 << BASELINE_LEAF_INDEX_BITS;
constexpr uint64_t BASELINE_LEAF_MASK = MAKE_MASK(BASELINE_LEAF_INDEX_BITS, uint64_t);
constexpr uint64_t BASELINE_CTR_INDEX_MASK = MAKE_MASK(CACHELINE_BITS, uint64_t);




//enum TraversePathStatus {
//	VERIFY_SUCCESS = SUCCESS,
//	VERIFY_FAILURE,
//	HASH_ROOT_MORE,
//	HASH_BREAK_VERIFY_CACHE_HIT,
//	HASH_BREAK_CACHE_EVICTE,
//	NOP
//};

enum class TraversePathType {
	NORMAL, //写操作正常写缓存
	FORCE //写操作全部强制写回内存
};

constexpr status VERIFY_SUCCESS = 0;
constexpr status VERIFY_FAILURE = 1;
constexpr status HASH_ROOT_MORE = 2;

typedef status(*hash_func_t)(hash_t* root, cacheline_t cacheline, uint32_t n);

//这里的root是寄存器
status hash_func_UT(hash_t* root, cacheline_t cacheline, uint32_t n);




//像这种cacheline一般都是CACHELINE对齐的SIZE_OF_NODE == CACHELINE
//而root则通常不是cacheline对齐
status hash_func_verify(hash_t* root, cacheline_t cacheline, uint32_t n);

// 对cacheline_register里面的数算哈希值
status hash_func_verify_using_large_reg(hash_t* root, cacheline_t cacheline, uint32_t n);

///dr_tree使用
//还有一种写操作，是只将数据写入缓存中。
//这里的root基本上是个内存地址，除非特殊的全局根，根可以放到缓存中一个特殊地方，保证每次都命中
//该函数运行完，可以将全局根的值即刻写到寄存器
status hash_func_update(hash_t* root, cacheline_t cacheline, uint32_t n);

///baseline使用,忽略更新时间
status hash_func_background_update(hash_t* root, cacheline_t cacheline, uint32_t n);

//strict persist使用
status hash_func_flush_update(hash_t* root, cacheline_t cacheline, uint32_t n);

//atomic update使用
status hash_func_check_acc_update(hash_t* root, cacheline_t cacheline, uint32_t n);

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


//一些辅助的元数据管理
struct BMTree {
	addr_t base = 0;
	addr_t root = 0;
	uint32_t leaf_N = 0;
	uint32_t sizeof_node = SIZE_OF_NODE;
	uint32_t BMT_N = 1;
	uint32_t sizeof_BMT = 0;
	hash_func_t update_hash = hash_func_update;
	hash_func_t verify_hash = hash_func_verify_using_large_reg;
};

//申请空间，构造BMT_N个存储空间连续的树，需要连续存放么？
constexpr int g_exp = 3;
#define next_level_N(cur_leaf_N, exp) ((cur_leaf_N + (1 << g_exp) - 1) >> 3)
#define next_level_idx(cur_leaf_N, exp) (cur_leaf_N >> 3)

uint32_t BMT_get_tree_node_N(uint32_t leaf_N);

//两个输出参数
status __BMT_alloc(addr_t* base, uint32_t* BMT_size, uint32_t leaf_N,
	uint32_t sizeof_node = SIZE_OF_NODE, uint32_t BMT_N = 1);

status __BMT_free(addr_t base);

status BMT_alloc(BMTree& bmTree);

status BMT_free(BMTree& bmTree);

status __BMT_traverse_tree(addr_t base, uint32_t leaf_N, addr_t root,
	uint32_t sizeof_node, hash_func_t hash, uint32_t valid_leaf_N);

status __BMT_traverse_tree(addr_t base, uint32_t leaf_N, addr_t root,
	uint32_t sizeof_node, uint32_t BMT_N, uint32_t sizeof_BMT, hash_func_t hash, uint32_t valid_leaf_N);

//后面可能还有很多其他的要处理
status __BMT_traverse_path(addr_t base, uint32_t leaf_N, uint32_t leaf_K, addr_t root,
	uint32_t sizeof_node, hash_func_t hash, uint32_t valid_leaf_N);

status BMT_construct(BMTree& bmTree);

status BMT_construct(BMTree& bmTree, uint32_t valid_leaf_N);

//这个是root顺序对应的,back_tree可以调用这个接口
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K);

//一些特殊的方案可能需要灵活的hash函数
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, hash_func_t hash);

//region_tree和back_tree可以调用这个接口,会用到cacheline_register
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root);

//fore_tree需要调用该接口。会用到cacheline_register
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K,
	addr_t root, uint32_t valid_leaf_N);

//verify的过程，查找counter是否在缓存中，在则不用验证，否则先读到cache中，
//或者特殊的cacheline寄存器，再调用该函数，以便不用再次发起访存。
//back_tree可以调用这个接口
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K);

//region_tree和back_tree可以调用这个接口,会用到cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root);

//fore_tree需要调用该接口。cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K,
	addr_t root, uint32_t valid_leaf_N);

status __BMT_calculate_root(addr_t base, uint32_t leaf_N, uint32_t sizeof_node,
	uint32_t valid_leaf_N, addr_t* root);

status BMT_get_root_for_valid_leaf_N(BMTree& bmTree,
	uint32_t valid_leaf_N, addr_t* root);

status BMT_get_ctr_attr(BMTree& bmTree, phy_addr_t addr,
	addr_t* ctr, uint32_t* BMT_K, uint32_t* leaf_K, uint32_t* I);

//由于baselineBMT是一整棵树并且有固定层数，所以这里单独写一个找CTR索引的函数
status BMT_get_ctr_attr_baseline(BMTree& bmTree, phy_addr_t addr,
	addr_t* ctr, uint32_t* leaf_K, uint32_t* I);

status BMT_get_leaf_K_addr(BMTree& bmTree, uint32_t leak_K, addr_t* addr);

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
	uint32_t I, uint64_t value);
void __BMT_fmt_print(const BMTree& bmTree);
status BMT_fmt_print(const BMTree& bmTree);

#endif // BMT_PROJECT_DEBUG

//constexpr uint64_t MEMORY_RANGE = (1LLU << 40); //1T内存
//constexpr uint64_t SEGMENT = (1U << 27); //128M
//constexpr uint64_t FULL_SEGMENT_NUM = (MEMORY_RANGE / SEGMENT);
//constexpr uint64_t HOT_SEGMENT_NUM = 512; //512个根

class BMTreeMgr {
public:
	static BMTreeMgr& get_instance() {
		static  BMTreeMgr instance;
		return instance;
	}

	BMTree baseline_BMTree();

	BMTreeMgr();
	~BMTreeMgr() = default;

public:
	BMTreeMgr(const BMTreeMgr&) = delete;
	BMTreeMgr(MetaMgr&&) = delete;
	BMTreeMgr& operator=(const BMTreeMgr&) = delete;
	BMTreeMgr& operator=(BMTreeMgr&&) = delete;

private:
	BMTree __baseline_BMTree;
};

extern hash_func_t g_hash;


extern BMTree g_baseline_BMTree;
extern addr_t g_baseline_BMTree_root;

#endif