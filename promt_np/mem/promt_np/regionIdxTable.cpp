#include "regionIdxTable.h"

//Statistics& g_Statistics = Statistics::get_instance();


/*Tool func*/
/*RegisterTool*/
status 
__RegisterTool::get_value(table_size_t idx_K_bits, uint64_t mask, uint64_t* value) {
	table_size_t idx = idx_K_bits >> 3;
	table_size_t K = idx_K_bits & 0x7;
	if (idx >= BOARD_BYTES) { //�����߽�
		K += ((idx - BOARD_BYTES) << 3);
		idx = BOARD_BYTES;
	}
	uint64_t data = *(uint64_t*)(cacheline + idx);
	*value = (data >> K) & mask; //������value���Ƿ���cache������Ƿ��ʼĴ���
	return SUCCESS;
}

//��cacheline�ĵ�idx_K_bits��bit��ʼ��64bit���ݣ�����Ϊmask����ָʾ��64bit���ö೤bit����valueֵ��
//status set_value(table_size_t idx_K_bits, table_size_t mask, uint64_t value, size_t lat_factor = 1) {
status 
__RegisterTool::set_value(table_size_t idx_K_bits, uint64_t mask, uint64_t value, bool write_back) {
	table_size_t idx = idx_K_bits >> 3;
	table_size_t K = idx_K_bits & 0x7;
	if (idx >= BOARD_BYTES) { //�����߽�
		K += ((idx - BOARD_BYTES) << 3);
		idx = BOARD_BYTES;
	}
	value &= mask;
	value <<= K;
	uint64_t erase = MAKE_ZERO_ERASE(mask, K, uint64_t);

	uint64_t* data = (uint64_t*)(cacheline + idx); //����cache
	*data = (*data & erase) | value; //����cache
	return SUCCESS;
}

