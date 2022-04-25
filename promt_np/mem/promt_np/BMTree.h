#ifndef __PROMT_BMTREE_HH__
#define __PROMT_BMTREE_HH__

#include "common.h"
#include "metaMgr.h"
#include "unionCtr.h"
#include <stdint.h>
#include <functional>
#include <string>
//������ʱ��Ĵ�����ռ,��root���ܿ�cacheline
//��������·��ʱ��root�����Ѿ��ڷ���Ҷ��ʱ������ϡ�
//#define LARGE_REGISTER_LINK_OPTI

//����ٽ�������ʵ�ַֿ�

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

//ProMT-NP����
constexpr uint64_t PROMT_GLOBAL_BMT_LEVEL = 9;
constexpr uint64_t PROMT_GLOBAL_LEAF_INDEX_BITS = 24;
constexpr uint64_t PROMT_GLOBAL_LEAF_N = 1 << PROMT_GLOBAL_LEAF_INDEX_BITS;
constexpr uint64_t PROMT_CTR_INDEX_BITS = 6;//һ��cacheline��64��������
constexpr uint64_t PROMT_GLOBAL_LEAF_MASK = MAKE_MASK(PROMT_GLOBAL_LEAF_INDEX_BITS, uint64_t);
constexpr uint64_t PROMT_CTR_INDEX_MASK = MAKE_MASK(CACHELINE_BITS, uint64_t);

constexpr uint64_t PROMT_HOT_BMT_LEVEL = 4;//ʵ������3
constexpr uint64_t PROMT_HOT_LEAF_INDEX_BITS = 9; //512��Ҷ�ӽڵ㣬���ڵ���64��
constexpr uint64_t PROMT_HOT_TREE_INDEX_BITS = 6;
constexpr uint64_t PROMT_HOT_REAL_LEAF_N = 1 << PROMT_HOT_LEAF_INDEX_BITS;
constexpr uint64_t PROMT_HOT_LEAF_N = 1 << PROMT_HOT_TREE_INDEX_BITS;
constexpr uint64_t PROMT_HOT_LEAF_MASK = MAKE_MASK(PROMT_HOT_LEAF_INDEX_BITS, uint64_t);

constexpr uint64_t VICTIM_QUEUE_INDEX = 3; //Q3ͷ��victim node

//enum TraversePathStatus {
//	VERIFY_SUCCESS = SUCCESS,
//	VERIFY_FAILURE,
//	HASH_ROOT_MORE,
//	HASH_BREAK_VERIFY_CACHE_HIT,
//	HASH_BREAK_CACHE_EVICTE,
//	NOP
//};

enum class TraversePathType {
	NORMAL, //д��������д����
	FORCE //д����ȫ��ǿ��д���ڴ�
};

constexpr status VERIFY_SUCCESS = 0;
constexpr status VERIFY_FAILURE = 1;
constexpr status HASH_ROOT_MORE = 2;

typedef status(*hash_func_t)(hash_t* root, cacheline_t cacheline, uint32_t n);

//�����root�ǼĴ���
status hash_func_UT(hash_t* root, cacheline_t cacheline, uint32_t n);




//������cachelineһ�㶼��CACHELINE�����SIZE_OF_NODE == CACHELINE
//��root��ͨ������cacheline����
status hash_func_verify(hash_t* root, cacheline_t cacheline, uint32_t n);

// ��cacheline_register����������ϣֵ
status hash_func_verify_using_large_reg(hash_t* root, cacheline_t cacheline, uint32_t n);


///dr_treeʹ��
//����һ��д��������ֻ������д�뻺���С�
//�����root�������Ǹ��ڴ��ַ�����������ȫ�ָ��������Էŵ�������һ������ط�����֤ÿ�ζ�����
//�ú��������꣬���Խ�ȫ�ָ���ֵ����д���Ĵ���
status hash_func_update(hash_t* root, cacheline_t cacheline, uint32_t n);


//strict persistʹ��
status hash_func_flush_update(hash_t* root, cacheline_t cacheline, uint32_t n);




//һЩ������Ԫ���ݹ���
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

//����ռ䣬����BMT_N���洢�ռ�������������Ҫ�������ô��
constexpr int g_exp = 3;
#define next_level_N(cur_leaf_N, exp) ((cur_leaf_N + (1 << g_exp) - 1) >> 3)
#define next_level_idx(cur_leaf_N, exp) (cur_leaf_N >> 3)

uint32_t BMT_get_tree_node_N(uint32_t leaf_N);

//�����������
status __BMT_alloc(addr_t* base, uint32_t* BMT_size, uint32_t leaf_N,
	uint32_t sizeof_node = SIZE_OF_NODE, uint32_t BMT_N = 1);

status __BMT_free(addr_t base);

status BMT_alloc(BMTree& bmTree);

status BMT_free(BMTree& bmTree);

status __BMT_traverse_tree(addr_t base, uint32_t leaf_N, addr_t root,
	uint32_t sizeof_node, hash_func_t hash, uint32_t valid_leaf_N);

status __BMT_traverse_tree(addr_t base, uint32_t leaf_N, addr_t root,
	uint32_t sizeof_node, uint32_t BMT_N, uint32_t sizeof_BMT, hash_func_t hash, uint32_t valid_leaf_N);

//������ܻ��кܶ�������Ҫ����
status __BMT_traverse_path(addr_t base, uint32_t leaf_N, uint32_t leaf_K, addr_t root,
	uint32_t sizeof_node, hash_func_t hash, uint32_t valid_leaf_N);

status BMT_construct(BMTree& bmTree);

status BMT_construct(BMTree& bmTree, uint32_t valid_leaf_N);


//һЩ����ķ���������Ҫ����hash����
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, hash_func_t hash);

//hot_tree��Ҫ���øýӿڣ����õ�cacheline_register
//���ⲿ��֤һ������Ϊhot tree��Ҷ�ӽڵ�ʵ��������global tree��
status BMT_path_update(const BMTree& bmTree_hot, const BMTree& bmTree_region,
	uint32_t leaf_K, uint32_t hot_index, addr_t root);

status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, addr_t root, uint32_t valid_leaf_N);

status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root);

status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root);

//region_tree��back_tree���Ե�������ӿ�,���õ�cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root);

// hot tree��verify֮ǰ��Ҷ�ӽڵ��Ѿ������˼Ĵ�����
// Ŀǰ��֪�����½ڵ���hot tree�е�λ��(hot_index\leaf_K)�����Լ��������Ӧ��һ���ĸ��ڵ�
// �����leaf_K��region tree��leaf�ı�ţ�24λ��ʵ����ҳ��ַֻ��22λ������ֱ����hot 
// ��ʱ��leaf_K������ʵ�����ǵڶ��㣿Ȼ��һֱ������֤
status BMT_verify_counter(const BMTree& bmTree_hot, const BMTree& bmTree_region, 
	uint32_t leaf_K, uint32_t hot_index, addr_t root);

status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, addr_t root, uint32_t valid_leaf_N);

status __BMT_calculate_root(addr_t base, uint32_t leaf_N, uint32_t sizeof_node,
	uint32_t valid_leaf_N, addr_t* root);

status BMT_get_root_for_valid_leaf_N(BMTree& bmTree,
	uint32_t valid_leaf_N, addr_t* root);

status BMT_get_ctr_attr(BMTree& bmTree, phy_addr_t addr,
	addr_t* ctr, uint32_t* BMT_K, uint32_t* leaf_K, uint32_t* I);


//����globalBMT��һ�����������й̶��������������ﵥ��дһ����CTR�����ĺ���
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