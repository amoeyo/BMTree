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

//û�д�Ĵ�������Ҫ��κ�cache������ɼ���
struct MemoryUnionCtr {
	//����̶�Ϊ7bit��maskΪ0x7f,��RegionIdxTable��һ������
	uint8_t __minor[MINOR_RANGE];
	major_t __major; //��__minor��һ���洢λ��
	status getValue(uint32_t ctr_K, major_t* major, uint64_t* minor);

	status __set_major(uint64_t major);
	status __get_major(uint64_t* major);

	status __increase_major();

	status __set_K_minor(table_size_t idx_K, uint64_t value);
	status __get_K_minor(table_size_t idx_K, uint64_t* value);
	status set_K_minor(table_size_t idx_K, uint64_t minor);
};



//С�ͳ־���cache
using CPTA_t = class Counter_Persistent_Register_Array {
private:
	typedef struct Persistent_register {
		phy_addr_t __tag : N_PERSISTENT_REGISTER_TAG_BITS;
		phy_addr_t __count : N_PERSISTENT_REGISTER_COUNT_BITS;
		phy_addr_t __line_valid : 1; //__valid����...
		phy_addr_t __dirty : 1;
		uint8_t __data[CACHELINE];
	} Persistent_register_t;
public:
	static Counter_Persistent_Register_Array& get_instance() {
		static  Counter_Persistent_Register_Array instance;
		return instance;
	}
public:
	//Ӧ�ò�����øýӿ�
	status read(void* addr, void* buffer, size_t n_bytes, eviction_cb_t eviction_cb);

	//�������Ҫ��һ���ص�����
	status write(void* addr, void* buffer, size_t n_bytes,
		eviction_cb_t eviction_cb, bool write_back = false);
private:
	status hit_fresh(size_t hit_index);

	status select_pst_reg(size_t* selected_index);
	//�����Ӧ���ǲ�����ʲô�ӳٵ�
	status __read(phy_addr_t addr, addr_t buffer, size_t n_bytes, eviction_cb_t eviction_cb);

	status __write(phy_addr_t addr, addr_t buffer, size_t n_bytes, eviction_cb_t eviction_cb);

public:
	Counter_Persistent_Register_Array(const Counter_Persistent_Register_Array&) = delete;
	Counter_Persistent_Register_Array(Counter_Persistent_Register_Array&&) = delete;
	Counter_Persistent_Register_Array& operator=(const Counter_Persistent_Register_Array&) = delete;
	Counter_Persistent_Register_Array& operator=(Counter_Persistent_Register_Array&&) = delete;

public:
	Counter_Persistent_Register_Array();
	~Counter_Persistent_Register_Array() = default;

private:
	Persistent_register pst_reg_array[N_CTR_PERSISTENT_REGISTER];
};

status reg_data_move(void* dst, void* src, size_t n_bytes);

status read_pst_reg(void* addr, void* buffer, size_t n_bytes,
	eviction_cb_t eviction_cb = nullptr);

status write_pst_reg(void* addr, void* buffer, size_t n_bytes,
	eviction_cb_t eviction_cb = nullptr, bool write_back = false);


#endif