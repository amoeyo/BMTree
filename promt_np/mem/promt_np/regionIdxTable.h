#ifndef __REGION_TABLE_HH__
#define __REGION_TABLE_HH__

#include "common.h"
#include "cache.h"
//�޷�����
using table_size_t = uint64_t;
constexpr uint64_t CACHELINE_END = (CACHELINE - 1);
constexpr uint64_t BOARD_BYTES = (CACHELINE - sizeof(uint64_t));

//���ʹ�ú����������ȸ�ֵ
#define CEIL_DIV(__x, __y) (((__x) + (__y) - 1) / (__y))



//������������״̬λ������
//��Ĵ���ģʽ
struct __RegisterTool {
	uint8_t cacheline[CACHELINE];
	status get_value(table_size_t idx_K_bits, uint64_t mask, uint64_t* value);
	status set_value(table_size_t idx_K_bits, uint64_t mask, uint64_t value, bool write_back = false);
};


#endif

