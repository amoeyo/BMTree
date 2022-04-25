#include "statistics.h"


Statistics g_Statistics;

Statistics::Statistics() : __t_latency(0), __hash_latency(0), __memory_read_latency(0),
__memory_write_latency(0), __cache_latency(0),
__hash_cntr(0),
__cache_miss_cntr(0), __cache_read_miss_cntr(0), __cache_write_miss_cntr(0),
__cache_hit_cntr(0), __cache_read_hit_cntr(0), __cache_write_hit_cntr(0),
__cache_read_called_cntr(0), __cache_write_called_cntr(0),
__extra_cache_miss_cntr(0), __extra_cache_hit_cntr(0),
__cache_read_eviction_cntr(0), __cache_write_eviction_cntr(0), __cache_eviction_cntr(0),
__extra_cache_eviction_cntr(0),
__pst_reg_miss_cntr(0), __pst_reg_hit_cntr(0),
__user_read_cntr(0), __engine_extra_mem_bmt_read_cntr(0), __engine_extra_mem_index_read_cntr(0),
__user_write_cntr(0), __engine_extra_mem_bmt_write_cntr(0), __engine_extra_mem_index_write_cntr(0),
__mem_write_cycle_cntr(0),
__accelerate_bmt_recover_table_cntr(0),
__extra_strict_persist_write_cntr(0), __counter_overflow_write_cntr(0) { };

latency_t
Statistics::get_t_latency() { return __t_latency; }

void
Statistics::set_t_latency(latency_t lat) { __t_latency = lat; }

cntr_t Statistics::get_total_write_cntr() { return __user_write_cntr; }

cntr_t
Statistics::get_user_write_cntr() { return __user_write_cntr; }
cntr_t
Statistics::get_user_read_cntr() { return __user_read_cntr; }
cntr_t
Statistics::get_extra_engine_write_cntr() {
	return __engine_extra_mem_bmt_write_cntr + __engine_extra_mem_index_write_cntr;
}
cntr_t
Statistics::get_extra_engine_read_cntr() {
	return __engine_extra_mem_bmt_read_cntr + __engine_extra_mem_index_read_cntr;
}

cntr_t 
Statistics::get_meta_extra_write_cntr() { return __engine_extra_mem_bmt_write_cntr; }
cntr_t 
Statistics::get_meta_extra_read_cntr() { return __engine_extra_mem_bmt_read_cntr; }
void
Statistics::set_meta_extra_write_cntr(cntr_t cntr) { __engine_extra_mem_bmt_write_cntr = cntr; }
void 
Statistics::set_meta_extra_read_cntr(cntr_t cntr) { __engine_extra_mem_bmt_read_cntr = cntr; }

cntr_t
Statistics::get_hash_cntr() { return __hash_cntr; }
cntr_t
Statistics::get_cache_hit_cntr() { return __cache_hit_cntr; }
cntr_t
Statistics::get_cache_miss_cntr() { return __cache_miss_cntr; }

void
Statistics::set_cache_hit_cntr(cntr_t cntr) { __cache_hit_cntr = cntr; }
void
Statistics::set_cache_miss_cntr(cntr_t cntr) { __cache_miss_cntr = cntr; }

cntr_t
Statistics::get_extra_cache_hit_cntr() { return __extra_cache_hit_cntr; }
cntr_t
Statistics::get_extra_cache_miss_cntr() { return __extra_cache_miss_cntr; }

cntr_t
Statistics::get_extra_strict_persist_write_cntr() { return __extra_strict_persist_write_cntr; }
cntr_t
Statistics::get_counter_overflow_write_cntr() { return __counter_overflow_write_cntr; }
cntr_t
Statistics::get_acc_table_writes_cntr() { return __accelerate_bmt_recover_table_cntr; }
cntr_t
Statistics::get_meta_cache_eviction_cntr() { return __cache_eviction_cntr; }
void 
Statistics::set_meta_cache_eviction_cntr(cntr_t cntr) { __cache_eviction_cntr = cntr; }
cntr_t
Statistics::get_extra_cache_eviction_cntr() { return __extra_cache_eviction_cntr; }

void
Statistics::t_lat_add(latency_t lat) { __t_latency += lat; }
void
Statistics::hash_lat_add(latency_t lat) { __hash_latency += lat; }

void
Statistics::memory_read_lat_add(latency_t lat) { __memory_read_latency += lat; }
void
Statistics::memory_write_lat_add(latency_t lat) { __memory_write_latency += lat; }
void
Statistics::cache_lat_add(latency_t lat) { __cache_latency += lat; }

void
Statistics::inc_hash_cntr() { ++__hash_cntr; }

void
Statistics::inc_cache_miss_cntr() { ++__cache_miss_cntr; }
void
Statistics::inc_cache_read_miss_cntr() { ++__cache_read_miss_cntr; }
void
Statistics::inc_cache_write_miss_cntr() { ++__cache_write_miss_cntr; }

void
Statistics::inc_cache_hit_cntr() { ++__cache_hit_cntr; }
void
Statistics::inc_cache_read_hit_cntr() { ++__cache_read_hit_cntr; }
void
Statistics::inc_cache_write_hit_cntr() { ++__cache_write_hit_cntr; }



void
Statistics::inc_cache_read_called_cntr() { ++__cache_read_called_cntr; }
void
Statistics::inc_cache_write_called_cntr() { ++__cache_write_called_cntr; }

void
Statistics::inc_extra_cache_hit_cntr() { ++__extra_cache_hit_cntr; }
void
Statistics::inc_extra_cache_miss_cntr() { ++__extra_cache_miss_cntr; }

void
Statistics::inc_cache_read_eviction_cntr() { ++__cache_read_eviction_cntr; }
void
Statistics::inc_cache_write_eviction_cntr() { ++__cache_write_eviction_cntr; }
void
Statistics::inc_cache_eviction_cntr() { ++__cache_eviction_cntr; }

void
Statistics::inc_extra_cache_eviction_cntr() { ++__extra_cache_eviction_cntr; }

void
Statistics::inc_pst_reg_miss_cntr() { ++__pst_reg_miss_cntr; }
void
Statistics::inc_pst_reg_hit_cntr() { ++__pst_reg_hit_cntr; }

void
Statistics::inc_user_read_cntr() { ++__user_read_cntr; }
void
Statistics::inc_engine_extra_mem_bmt_read_cntr() { ++__engine_extra_mem_bmt_read_cntr; }
void
Statistics::inc_engine_extra_mem_index_read_cntr() { ++__engine_extra_mem_index_read_cntr; }

void
Statistics::inc_user_write_cntr() { ++__user_write_cntr; }
void
Statistics::inc_engine_extra_mem_bmt_write_cntr() { ++__engine_extra_mem_bmt_write_cntr; }
void
Statistics::inc_engine_extra_mem_index_write_cntr() { ++__engine_extra_mem_index_write_cntr; }

void
Statistics::inc_mem_write_cycle_cntr() { ++__mem_write_cycle_cntr; }
void
Statistics::clear_mem_write_cycle_cntr() { __mem_write_cycle_cntr = 0; }
cntr_t
Statistics::get_mem_write_cycle_cntr() { return __mem_write_cycle_cntr; }

void
Statistics::inc_accelerate_bmt_recover_table_cntr() { ++__accelerate_bmt_recover_table_cntr; }

void Statistics::inc_extra_strict_persist_write_cntr() { ++__extra_strict_persist_write_cntr; }
void Statistics::inc_counter_overflow_write_cntr() { ++__counter_overflow_write_cntr; }

void
Statistics::pretty_show() {}