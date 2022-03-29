#ifndef __GLOBAL_STATISTICS_HH__
#define __GLOBAL_STATISTICS_HH__

#include "common.h"

using cntr_t = uint64_t;
//using cntr_t = std::atomic<uint64_t>;
using latency_t = uint64_t;

constexpr latency_t hash_latency_unit = 80;
constexpr latency_t cache_latency_unit = 2;
constexpr latency_t memory_read_latency_unit = 60;
constexpr latency_t memory_write_latency_unit = 150;
constexpr latency_t calculate_otp_latency_unit = 40;
constexpr latency_t parallel_exec_extra_latency_unit
						= (memory_read_latency_unit - calculate_otp_latency_unit - cache_latency_unit);
constexpr latency_t parallel_memory_read_latency_unit = 12;//并行从NVM中读数据

class Statistics {
private:
	latency_t __t_latency; //总延迟
	latency_t __hash_latency; //计算hash产生的延迟
	latency_t __memory_read_latency;//访存读总延迟
	latency_t __memory_write_latency;//访存写总延迟
	latency_t __cache_latency;//访问cache延迟

private:
	cntr_t __hash_cntr;

	cntr_t __cache_miss_cntr;
	cntr_t __cache_read_miss_cntr;
	cntr_t __cache_write_miss_cntr;
	cntr_t __cache_hit_cntr;
	cntr_t __cache_read_hit_cntr;
	cntr_t __cache_write_hit_cntr;
	cntr_t __cache_read_called_cntr; //接口被调用次数，一次调用可能跨越多个cacheline
	cntr_t __cache_write_called_cntr;

	cntr_t __cache_read_eviction_cntr;
	cntr_t __cache_write_eviction_cntr;
	cntr_t __cache_eviction_cntr;

	cntr_t __pst_reg_miss_cntr;
	cntr_t __pst_reg_hit_cntr;

	cntr_t __user_read_cntr;
	cntr_t __engine_extra_mem_bmt_read_cntr;//总的加密引擎引入的读访存次数，cache不命中时带来的
	cntr_t __engine_extra_mem_index_read_cntr; //这个可能100%命中...暂时不统计

	cntr_t __user_write_cntr;
	cntr_t __engine_extra_mem_bmt_write_cntr; //总的加密引擎引入的写访存次数，包括写穿(包括back_index_table的索引写穿)，驱逐。
	cntr_t __engine_extra_mem_index_write_cntr; //仅back_index_table的带来的写穿和可能的驱逐

	cntr_t __accelerate_bmt_recover_table_cntr;

	//cntr_t __bmt_read_hit_cntr;
	//cntr_t __bmt_read_miss_cntr;
//private:
public:
	Statistics();
	~Statistics() = default;


public:
	latency_t get_t_latency();
	void set_t_latency(latency_t lat);
	cntr_t get_user_write_cntr();
	cntr_t get_user_read_cntr();
	cntr_t get_extra_engine_write_cntr();
	cntr_t get_extra_engine_read_cntr();
	cntr_t get_hash_cntr();
	cntr_t get_cache_hit_cntr();
	cntr_t get_cache_miss_cntr();

public:
	void t_lat_add(latency_t lat);
	void hash_lat_add(latency_t lat);
	void memory_read_lat_add(latency_t lat);
	void memory_write_lat_add(latency_t lat);
	void cache_lat_add(latency_t lat);

	void inc_hash_cntr();

	void inc_cache_miss_cntr();
	void inc_cache_read_miss_cntr();
	void inc_cache_write_miss_cntr();

	void inc_cache_hit_cntr();
	void inc_cache_read_hit_cntr();
	void inc_cache_write_hit_cntr();

	void inc_cache_read_called_cntr();
	void inc_cache_write_called_cntr();

	void inc_cache_read_eviction_cntr();
	void inc_cache_write_eviction_cntr();
	void inc_cache_eviction_cntr();

	void inc_pst_reg_miss_cntr();
	void inc_pst_reg_hit_cntr();

	void inc_user_read_cntr();
	void inc_engine_extra_mem_bmt_read_cntr();
	void inc_engine_extra_mem_index_read_cntr();

	void inc_user_write_cntr();
	void inc_engine_extra_mem_bmt_write_cntr();
	void inc_engine_extra_mem_index_write_cntr();

	void inc_accelerate_bmt_recover_table_cntr();

	void pretty_show();
public:
	/*static Statistics& get_instance() {
		static  Statistics instance;
		return instance;
	}*/

public:
	Statistics(const Statistics&) = delete;
	Statistics(Statistics&&) = delete;
	Statistics& operator=(const Statistics&) = delete;
	Statistics& operator=(Statistics&&) = delete;
};

extern Statistics g_Statistics;

#endif