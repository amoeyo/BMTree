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


#endif

