#ifndef __PROMT_RW_HH__
#define __PROMT_RW_HH__

#include "BMTree.h"
#include "metaMgr.h"
#include "descriptor.h"

using Tick = uint64_t;
// 2 bits state
// 00 clean + invalid
// 01 clean + valid
// 11 dirty + valid
// 没有10这种状态
//constexpr uint64_t STATE_VALID_AND_DIRTY = ~(uint64_t(0));
//constexpr uint64_t STATE_VALID_AND_CLEAN = 0x7777777777777777;
//恰好是整数倍



constexpr size_t n_state_cachelines = ((FORE_SEGMENT_NUM * FORE_STATE_BITS) >> 3 >> CACHELINE_BITS);
constexpr size_t n_states_per_cacheline = (CACHELINE << 3) / FORE_STATE_BITS;
//constexpr size_t n_states_per_qword = (sizeof(uint64_t) << 3) / FORE_STATE_BITS;
//constexpr size_t n_state_qwords_per_cacheline = (CACHELINE) / sizeof(uint64_t);
constexpr size_t n_state_per_eviction_segments = 8;
constexpr size_t n_eviction_segments = FORE_SEGMENT_NUM / n_state_per_eviction_segments;//每16个作为一个驱逐组
constexpr size_t n_counter_write_back_threshold = 16;
constexpr size_t n_eviction_tick_threshold = 64;
constexpr double update_g_valid_leaf_N_factor = 0.67; //三分之二

constexpr uint64_t CLEAN_INVALID = 0x0;
constexpr uint64_t CLEAN_VALID = 0x1;
constexpr uint64_t DIRTY_VALID = 0x3;
constexpr uint64_t STATE_MASK = MAKE_MASK(FORE_STATE_BITS, uint64_t);
constexpr uint64_t EVICTION_SEGMENTS_MASK = n_eviction_segments - 1;

constexpr status ADD_NEW_NODE = 40;
constexpr status EVICTION_OLD_NODE = 41;

constexpr size_t LIFETIME = 16;

status promt_np_read(phy_addr_t addr, size_t n_bytes, Tick& read_latency);

status promt_np_write(phy_addr_t addr, size_t n_bytes, Tick& write_latency);

void print_hotpage_map();


#endif
