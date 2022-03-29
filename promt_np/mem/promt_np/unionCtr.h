#ifndef __UNIONCTR_HH__
#define __UNIONCTR_HH__

#include "common.h"
#include "regionIdxTable.h"
#include "cache.h"

#define IGNORE_MINOR_OVERFLOW
//����CACHELINE == 64,����Major��ʾ����
//һ�㽫��ַǿ������ת��������ģʽ
using major_t = uint64_t;
constexpr status MINOR_OVERFLOW = 15;
constexpr uint32_t MINOR_RANGE = (CACHELINE - sizeof(major_t));
constexpr uint32_t MINOR_BOARD_BYTES = (MINOR_RANGE - 2);
constexpr uint32_t MINOR_END = (MINOR_RANGE - 1);

constexpr uint32_t N_PERSISTENT_REGISTER_COUNT_BITS = 2; //�������BITS = 4
constexpr uint32_t N_PERSISTENT_REGISTER_TAG_BITS = (ADDR_LEN - CACHELINE_BITS); //�����ַ����λ��������
constexpr uint32_t N_CTR_PERSISTENT_REGISTER = (1 << N_PERSISTENT_REGISTER_COUNT_BITS);
constexpr uint32_t CTR_PERSISTENT_REGISTER_MAX = N_CTR_PERSISTENT_REGISTER - 1;
constexpr uint32_t PERSISTENT_REGISTER_TAG_OFFSET = ADDR_LEN - N_PERSISTENT_REGISTER_TAG_BITS;

#define __parse_pst_reg_tag(__addr) ((__addr) >> PERSISTENT_REGISTER_TAG_OFFSET)
#define __combine_pst_reg_addr(__tag) ((__tag) << PERSISTENT_REGISTER_TAG_OFFSET)


//�̶�ΪCACHELINE�ֽڣ���˿��Խ�Leaf���ͳ�UnionCtr���ͣ����ù��ߺ�������
//RegisterUnionCtr��ʵ��һ�������AlignedCacheline�ṹ������(AlignedCacheline*)thisת��Ȼ����ýӿڡ�
//��������Դ�Ĵ������еĲ���������������cache�ͷô棬����Ūһ��
//���������ȫ���Զ���Ӳ����ɣ���˳˳���������
struct RegisterUnionCtr {
	//����̶�Ϊ7bit��maskΪ0x7f,��RegionIdxTable��һ������
	uint8_t __minor[MINOR_RANGE];
	major_t __major; //��__minor��һ���洢λ��
	status getValue(uint32_t ctr_K, major_t* major, uint64_t* minor);
	void __set_major(major_t major);
	void __get_major(major_t* major);

	status __set_K_minor(table_size_t idx_K, uint64_t value);
	status __get_K_minor(table_size_t idx_K, uint64_t* value);
	status set_K_minor(table_size_t idx_K, uint64_t minor);
};




status reg_data_move(void* dst, void* src, size_t n_bytes);




#endif