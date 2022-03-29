#ifndef __BMTREE_METAMGR_HH__
#define __BMTREE_METAMGR_HH__


#include "common.h"
#include "regionIdxTable.h"

constexpr uint64_t BACK_INDEX_BITS = 10;
constexpr uint64_t FORE_BACK_STAT_BITS = 1;
constexpr uint64_t DR_IDNEX_BITS = BACK_INDEX_BITS - FORE_BACK_STAT_BITS; //512
constexpr uint64_t FORE_INDEX_BITS = 64;
constexpr uint64_t FORE_STATE_BITS = 2;
constexpr uint64_t BACK_REGION_INDEX_BITS = 32;//其实用不了这么多,只要region不会太小
constexpr uint64_t TICK_BITS = FORE_INDEX_BITS - BACK_REGION_INDEX_BITS; //感觉这个给大一点比较好
constexpr uint32_t INIT_VALID_LEAF_N = 8;

constexpr uint64_t DR_INDEX_MASK = MAKE_MASK(DR_IDNEX_BITS, uint64_t);
constexpr uint64_t FORE_BACK_STAT_MASK = MAKE_MASK(FORE_BACK_STAT_BITS, uint64_t);
constexpr uint64_t BACK_REGION_INDEX_MASK = MAKE_MASK(BACK_REGION_INDEX_BITS, uint64_t);
constexpr uint64_t TICK_MASK = MAKE_MASK(TICK_BITS, uint64_t);
constexpr uint64_t FORE_STATE_MASK = MAKE_MASK(FORE_STATE_BITS, uint64_t);

#define dr_index(__value) ((__value) & DR_INDEX_MASK)
#define fore_back_stat(__value) (((__value) >> DR_IDNEX_BITS) & FORE_BACK_STAT_MASK)
#define fore_tick(__value) ((__value) & TICK_MASK)
#define back_dr_index(__value) (((__value) >> TICK_BITS) & BACK_REGION_INDEX_MASK)
#define construct_fore_index(__index, __tick) ((((__index) & BACK_REGION_INDEX_MASK) << TICK_BITS) | ((__tick) & TICK_MASK))
#define construct_back_index(__state, __index) ((((__state) & FORE_BACK_STAT_MASK) << DR_IDNEX_BITS) | ((__index) & DR_INDEX_MASK))



class MetaMgr {
public:
	static MetaMgr& get_instance() {
		static  MetaMgr instance;
		return instance;
	}


public:
	MetaMgr(const MetaMgr&) = delete;
	MetaMgr(MetaMgr&&) = delete;
	MetaMgr& operator=(const MetaMgr&) = delete;
	MetaMgr& operator=(MetaMgr&&) = delete;

private:

	uint32_t __tick;
	uint32_t __valid_leaf_N; //fore_tree

public:
	MetaMgr();
	~MetaMgr() = default;
};

extern MetaMgr g_MetaMgr;
/*
extern AlignedRegionIdxTable g_back_index_table;
extern AlignedRegionIdxTable g_fore_index_table;
extern AlignedRegionIdxTable g_fore_state_table;
extern uint32_t g_tick_clock; //这个值应该随着处理的读写请求数增加
extern uint32_t g_valid_leaf_N;
*/

#endif