#ifndef __PROMT_BMTREE_HH__
#define __PROMT_BMTREE_HH__

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

//ProMT-NP方案
constexpr uint64_t PROMT_GLOBAL_BMT_LEVEL = 9;
constexpr uint64_t PROMT_GLOBAL_LEAF_INDEX_BITS = 24;
constexpr uint64_t PROMT_GLOBAL_LEAF_N = 1 << PROMT_GLOBAL_LEAF_INDEX_BITS;
constexpr uint64_t PROMT_CTR_INDEX_BITS = 6;//一个cacheline管64个计数器
constexpr uint64_t PROMT_GLOBAL_LEAF_MASK = MAKE_MASK(PROMT_GLOBAL_LEAF_INDEX_BITS, uint64_t);
constexpr uint64_t PROMT_CTR_INDEX_MASK = MAKE_MASK(CACHELINE_BITS, uint64_t);

constexpr uint64_t PROMT_HOT_BMT_LEVEL = 4;//实际上是3
constexpr uint64_t PROMT_HOT_LEAF_INDEX_BITS = 9; //512个叶子节点，父节点是64个
constexpr uint64_t PROMT_HOT_TREE_INDEX_BITS = 6;
constexpr uint64_t PROMT_HOT_REAL_LEAF_N = 1 << PROMT_HOT_LEAF_INDEX_BITS;
constexpr uint64_t PROMT_HOT_LEAF_N = 1 << PROMT_HOT_TREE_INDEX_BITS;
constexpr uint64_t PROMT_HOT_LEAF_MASK = MAKE_MASK(PROMT_HOT_LEAF_INDEX_BITS, uint64_t);

constexpr uint64_t VICTIM_QUEUE_INDEX = 3; //Q3头部victim node

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


//strict persist使用
status hash_func_flush_update(hash_t* root, cacheline_t cacheline, uint32_t n);




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


//一些特殊的方案可能需要灵活的hash函数
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, hash_func_t hash);

//hot_tree需要调用该接口，会用到cacheline_register
//在外部验证一次是因为hot tree的叶子节点实际上是在global tree中
status BMT_path_update(const BMTree& bmTree_hot, const BMTree& bmTree_region,
	uint32_t leaf_K, uint32_t hot_index, addr_t root);

status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, addr_t root, uint32_t valid_leaf_N);

status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root);

status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root);

//region_tree和back_tree可以调用这个接口,会用到cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root);

// hot tree的verify之前，叶子节点已经读到了寄存器中
// 目前已知：更新节点在hot tree中的位置(hot_index\leaf_K)，可以计算出它对应上一层哪个节点
// 这里的leaf_K是region tree上leaf的编号，24位，实际上页地址只有22位，可以直接用hot 
// 此时的leaf_K等数据实际上是第二层？然后一直往上验证
status BMT_verify_counter(const BMTree& bmTree_hot, const BMTree& bmTree_region, 
	uint32_t leaf_K, uint32_t hot_index, addr_t root);

status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, addr_t root, uint32_t valid_leaf_N);

status __BMT_calculate_root(addr_t base, uint32_t leaf_N, uint32_t sizeof_node,
	uint32_t valid_leaf_N, addr_t* root);

status BMT_get_root_for_valid_leaf_N(BMTree& bmTree,
	uint32_t valid_leaf_N, addr_t* root);

status BMT_get_ctr_attr(BMTree& bmTree, phy_addr_t addr,
	addr_t* ctr, uint32_t* BMT_K, uint32_t* leaf_K, uint32_t* I);


//由于globalBMT是一整棵树并且有固定层数，所以这里单独写一个找CTR索引的函数
status BMT_get_ctr_attr_global(BMTree& bmTree, phy_addr_t addr,
	addr_t* ctr, uint32_t* leaf_K, uint32_t* I);

status BMT_get_ctr_attr_I(phy_addr_t addr, uint32_t* I);

status BMT_get_leaf_K_addr(BMTree& bmTree, uint32_t leak_K, addr_t* addr);

class BMTreeMgr {
public:
	static BMTreeMgr& get_instance() {
		static  BMTreeMgr instance;
		return instance;
	}

	BMTree& global_BMTree();
	BMTree& hot_BMTree();

	BMTreeMgr();
	~BMTreeMgr() = default;

public:
	BMTreeMgr(const BMTreeMgr&) = delete;
	BMTreeMgr(MetaMgr&&) = delete;
	BMTreeMgr& operator=(const BMTreeMgr&) = delete;
	BMTreeMgr& operator=(BMTreeMgr&&) = delete;

private:
	BMTree __global_BMTree;
	BMTree __hot_BMTree;
};

extern hash_func_t g_hash;


extern BMTree g_global_BMTree;
extern addr_t g_global_BMTree_root;
extern BMTree g_hot_BMTree;
extern addr_t g_hot_BMTree_root;

#endif