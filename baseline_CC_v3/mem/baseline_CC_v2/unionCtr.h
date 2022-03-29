#ifndef __UNIONCTR_HH__
#define __UNIONCTR_HH__

#include "common.h"
#include "regionIdxTable.h"
#include "cache.h"

#define IGNORE_MINOR_OVERFLOW
//这里CACHELINE == 64,否则Major表示不下
//一般将地址强制类型转换成如下模式
using major_t = uint64_t;
constexpr status MINOR_OVERFLOW = 15;
constexpr uint32_t MINOR_RANGE = (CACHELINE - sizeof(major_t));
constexpr uint32_t MINOR_BOARD_BYTES = (MINOR_RANGE - 2);
constexpr uint32_t MINOR_END = (MINOR_RANGE - 1);

constexpr uint32_t N_PERSISTENT_REGISTER_COUNT_BITS = 2; //最大允许BITS = 4
constexpr uint32_t N_PERSISTENT_REGISTER_TAG_BITS = (ADDR_LEN - CACHELINE_BITS); //物理地址，高位舍弃才行
constexpr uint32_t N_CTR_PERSISTENT_REGISTER = (1 << N_PERSISTENT_REGISTER_COUNT_BITS);
constexpr uint32_t CTR_PERSISTENT_REGISTER_MAX = N_CTR_PERSISTENT_REGISTER - 1;
constexpr uint32_t PERSISTENT_REGISTER_TAG_OFFSET = ADDR_LEN - N_PERSISTENT_REGISTER_TAG_BITS;

#define __parse_pst_reg_tag(__addr) ((__addr) >> PERSISTENT_REGISTER_TAG_OFFSET)
#define __combine_pst_reg_addr(__tag) ((__tag) << PERSISTENT_REGISTER_TAG_OFFSET)


//固定为CACHELINE字节，因此可以将Leaf解释成UnionCtr类型，并用工具函数操作
//RegisterUnionCtr其实是一种特殊的AlignedCacheline结构，可以(AlignedCacheline*)this转换然后调用接口。
//由于是针对大寄存器进行的操作，因此无需访问cache和访存，单独弄一份
//这个部分完全可以独立硬件完成，因此乘除法不考虑
struct RegisterUnionCtr {
	//这里固定为7bit，mask为0x7f,是RegionIdxTable的一个特例
	uint8_t __minor[MINOR_RANGE];
	major_t __major; //和__minor换一个存储位置
	status getValue(uint32_t ctr_K, major_t* major, uint64_t* minor);
	void __set_major(major_t major);
	void __get_major(major_t* major);

	status __set_K_minor(table_size_t idx_K, uint64_t value);
	status __get_K_minor(table_size_t idx_K, uint64_t* value);
	status set_K_minor(table_size_t idx_K, uint64_t minor);
};

//没有大寄存器，需要多次和cache交互完成计算
struct MemoryUnionCtr {
	//这里固定为7bit，mask为0x7f,是RegionIdxTable的一个特例
	uint8_t __minor[MINOR_RANGE];
	major_t __major; //和__minor换一个存储位置
	status getValue(uint32_t ctr_K, major_t* major, uint64_t* minor);

	status __set_major(uint64_t major);
	status __get_major(uint64_t* major);

	status __increase_major();

	status __set_K_minor(table_size_t idx_K, uint64_t value);
	status __get_K_minor(table_size_t idx_K, uint64_t* value);
	status set_K_minor(table_size_t idx_K, uint64_t minor);
};



//小型持久型cache
using CPTA_t = class Counter_Persistent_Register_Array {
private:
	typedef struct Persistent_register {
		phy_addr_t __tag : N_PERSISTENT_REGISTER_TAG_BITS;
		phy_addr_t __count : N_PERSISTENT_REGISTER_COUNT_BITS;
		phy_addr_t __line_valid : 1; //__valid重名...
		phy_addr_t __dirty : 1;
		uint8_t __data[CACHELINE];
	} Persistent_register_t;
public:
	static Counter_Persistent_Register_Array& get_instance() {
		static  Counter_Persistent_Register_Array instance;
		return instance;
	}
public:
	//应该不会调用该接口
	status read(void* addr, void* buffer, size_t n_bytes, eviction_cb_t eviction_cb);

	//这里可能要加一个回调函数
	status write(void* addr, void* buffer, size_t n_bytes,
		eviction_cb_t eviction_cb, bool write_back = false);
private:
	status hit_fresh(size_t hit_index);

	status select_pst_reg(size_t* selected_index);
	//这个读应该是不带来什么延迟的
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