#ifndef __REGION_TABLE_HH__
#define __REGION_TABLE_HH__

#include "common.h"
#include "cache.h"
//无符号数
using table_size_t = uint64_t;
constexpr uint64_t CACHELINE_END = (CACHELINE - 1);
constexpr uint64_t BOARD_BYTES = (CACHELINE - sizeof(uint64_t));

//多次使用宏参数，最好先赋值
#define CEIL_DIV(__x, __y) (((__x) + (__y) - 1) / (__y))



//分配层索引表和状态位索引表
//大寄存器模式
struct __RegisterTool {
	uint8_t cacheline[CACHELINE];
	status get_value(table_size_t idx_K_bits, uint64_t mask, uint64_t* value);
	status set_value(table_size_t idx_K_bits, uint64_t mask, uint64_t value, bool write_back = false);
};
//内部工具类，固定为CACHELINE字节
struct __AlignedCacheline {
	uint8_t cacheline[CACHELINE];
	status get_value(table_size_t idx_K_bits, uint64_t mask, uint64_t* value);
	status set_value(table_size_t idx_K_bits, uint64_t mask, uint64_t value, bool write_back = false);
};

//本类型为以cacheline对齐的索引表
//特殊的unit_bits，没有必要用这个类型，比如8bits,16bits,32bits,64bits
//本类型最大支持(64 - 7)的unit_bits
//单独用缓存，就不要用一个特殊缓存实例了，就线性映射完事，表现为，直接加上特定的时延。
class AlignedRegionIdxTable {
public:
	AlignedRegionIdxTable(table_size_t idx_range, table_size_t unit_bits);

	//会发现这个函数，仅在访问__cacheilne_range这些私有变量会触发访问缓存开销
	status get_value(table_size_t idx_K, uint64_t* value);

	//提供两个cacheline操作
	status get_cachline(table_size_t cacheline_idx, cacheline_t buffer, uint32_t n_bytes = CACHELINE);

	//不带写穿接口，都是fore_index调用
	status write_cacheline(table_size_t cacheline_idx, cacheline_t buffer, uint32_t n_bytes = CACHELINE);

	status get_bytes(table_size_t bytes_idx, addr_t buffer, uint32_t n_bytes);

	status set_value(table_size_t idx_K, uint64_t value, bool write_back = false);

	~AlignedRegionIdxTable();
private:
	//这些私有变量的每次访问需要加缓存延迟，但是因为量很少，所以是否可以固定存到某些地方。
	addr_t __base;
	table_size_t __idx_range;
	table_size_t __unit_bits;
	table_size_t __cacheline_range;
	uint64_t __mask;
};

#endif

