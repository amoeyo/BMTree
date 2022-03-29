#ifndef __BASELINE_BMTREE_HH__
#define __BASELINE_BMTREE_HH__

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

//�Աȷ����̶ܹ���С���ڴ棬9��BMTreeҶ�ӽڵ���8^8��64B
constexpr uint64_t BASELINE_BMT_LEVEL = 9;
constexpr uint64_t BASELINE_LEAF_INDEX_BITS = 24;
constexpr uint64_t BASELINE_CTR_INDEX_BITS = 6;//һ��cacheline��64��������
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

///baselineʹ��,���Ը���ʱ��
status hash_func_background_update(hash_t* root, cacheline_t cacheline, uint32_t n);

//strict persistʹ��
status hash_func_flush_update(hash_t* root, cacheline_t cacheline, uint32_t n);

//atomic updateʹ��
status hash_func_check_acc_update(hash_t* root, cacheline_t cacheline, uint32_t n);

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

//�����root˳���Ӧ��,back_tree���Ե�������ӿ�
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K);

//һЩ����ķ���������Ҫ����hash����
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, hash_func_t hash);

//region_tree��back_tree���Ե�������ӿ�,���õ�cacheline_register
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root);

//fore_tree��Ҫ���øýӿڡ����õ�cacheline_register
status BMT_path_update(BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K,
	addr_t root, uint32_t valid_leaf_N);

//verify�Ĺ��̣�����counter�Ƿ��ڻ����У���������֤�������ȶ���cache�У�
//���������cacheline�Ĵ������ٵ��øú������Ա㲻���ٴη���ô档
//back_tree���Ե�������ӿ�
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K);

//region_tree��back_tree���Ե�������ӿ�,���õ�cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K, addr_t root);

//fore_tree��Ҫ���øýӿڡ�cacheline_register
status BMT_verify_counter(const BMTree& bmTree, uint32_t leaf_K, uint32_t BMT_K,
	addr_t root, uint32_t valid_leaf_N);

status __BMT_calculate_root(addr_t base, uint32_t leaf_N, uint32_t sizeof_node,
	uint32_t valid_leaf_N, addr_t* root);

status BMT_get_root_for_valid_leaf_N(BMTree& bmTree,
	uint32_t valid_leaf_N, addr_t* root);

status BMT_get_ctr_attr(BMTree& bmTree, phy_addr_t addr,
	addr_t* ctr, uint32_t* BMT_K, uint32_t* leaf_K, uint32_t* I);

//����baselineBMT��һ�����������й̶��������������ﵥ��дһ����CTR�����ĺ���
status BMT_get_ctr_attr_baseline(BMTree& bmTree, phy_addr_t addr,
	addr_t* ctr, uint32_t* leaf_K, uint32_t* I);

status BMT_get_leaf_K_addr(BMTree& bmTree, uint32_t leak_K, addr_t* addr);

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
	uint32_t I, uint64_t value);
void __BMT_fmt_print(const BMTree& bmTree);
status BMT_fmt_print(const BMTree& bmTree);

#endif // BMT_PROJECT_DEBUG

//constexpr uint64_t MEMORY_RANGE = (1LLU << 40); //1T�ڴ�
//constexpr uint64_t SEGMENT = (1U << 27); //128M
//constexpr uint64_t FULL_SEGMENT_NUM = (MEMORY_RANGE / SEGMENT);
//constexpr uint64_t HOT_SEGMENT_NUM = 512; //512����

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