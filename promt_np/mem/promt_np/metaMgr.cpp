#include "metaMgr.h"

/*
MetaMgr g_MetaMgr;

MetaMgr::MetaMgr() : __back_index_table(BACK_SEGMENT_NUM, BACK_INDEX_BITS),
		__fore_index_table(FORE_SEGMENT_NUM, FORE_INDEX_BITS),
		__fore_state_table(FORE_SEGMENT_NUM, FORE_STATE_BITS),
		__tick(0), __valid_leaf_N(INIT_VALID_LEAF_N) {

}

AlignedRegionIdxTable MetaMgr::back_index_table() { return __back_index_table; }
AlignedRegionIdxTable MetaMgr::fore_index_table() { return __fore_index_table; }
AlignedRegionIdxTable MetaMgr::fore_state_table() { return __fore_state_table; }
uint32_t MetaMgr::tick_clock() { return __tick; }
uint32_t MetaMgr::valid_leaf_N() { return __valid_leaf_N; }
*/

/*
AlignedRegionIdxTable g_back_index_table = g_MetaMgr.back_index_table();
AlignedRegionIdxTable g_fore_index_table = g_MetaMgr.fore_index_table();
AlignedRegionIdxTable g_fore_state_table = g_MetaMgr.fore_state_table();
uint32_t g_tick_clock = g_MetaMgr.tick_clock(); //这个值应该随着处理的读写请求数增加
uint32_t g_valid_leaf_N = g_MetaMgr.valid_leaf_N();
*/


/*以下定义一些对外接口*/