#ifndef __GLOBAL_CACHE_HH__
#define __GLOBAL_CACHE_HH__

#include "common.h"
#include "statistics.h"

constexpr size_t ADDR_LEN = (sizeof(phy_addr_t) << 3); //64
constexpr size_t CACHELINE_RANGE = 1024;//1024个cacheline,总体占空间约72KB以上
constexpr size_t CACHELINES_PER_WAY = PAGE / CACHELINE;//64个GROUP
constexpr size_t CACHELINE_WAYS = CACHELINE_RANGE / CACHELINES_PER_WAY; //16路组相联
constexpr size_t CACHELINE_WAYS_MAX = CACHELINE_WAYS - 1;
constexpr size_t CACHELINE_WAYS_BITS = 4;//多少bit可以计数WAYS,需要和CACHELINE_WAYS同步调整
//constexpr size_t CACHELINE_BITS = 6;//多少bit可以索引Cacheline内部具体某个字节,偏移为0
constexpr size_t CACHELINE_PER_WAY_BITS = 6;//多少bit可以索引组号,第6到12位，即从CACHELINE_OFFSET_BITS开始的6位
constexpr size_t CACHELINE_TAG_BITS = (ADDR_LEN - CACHELINE_PER_WAY_BITS - CACHELINE_BITS); //Tag占的bit数
constexpr size_t CACHELINE_PER_WAY_OFFSET_BITS = CACHELINE_BITS; //group索引偏移6位
constexpr size_t CACHELINE_TAG_OFFSET_BITS = (CACHELINE_PER_WAY_OFFSET_BITS + CACHELINE_PER_WAY_BITS); //Tag索引偏移12位

constexpr phy_addr_t GROUP_MASK = MAKE_MASK(CACHELINE_PER_WAY_BITS, phy_addr_t); //0x3f
constexpr phy_addr_t CACHELINE_BYTES_MASK = MAKE_MASK(CACHELINE_BITS, phy_addr_t); //0x3f

constexpr status PARAMETER_ERROR = 30;
constexpr status CACHE_HIT = 31;
constexpr status CACHE_MISS = 32;

extern uint8_t* cacheline_register; //64B共享大寄存器
extern uint8_t* cacheline_register_extra;
typedef status(*eviction_cb_t)(phy_addr_t addr);

#define VALID 1
#define INVALID (!(VALID))
#define DIRTY 1
#define CLEAN (!(DIRTY))


#define __parse_tag(__addr)  ((__addr) >> CACHELINE_TAG_OFFSET_BITS)
#define __parse_grp_idx(__addr) (((__addr) >> CACHELINE_PER_WAY_OFFSET_BITS) & GROUP_MASK)
#define __parse_bytes_idx(__addr) ((__addr) & CACHELINE_BYTES_MASK)
#define __parse_addr(__tag, __grp_idx, __byte_idx) (((__tag) << CACHELINE_TAG_OFFSET_BITS) | ((__grp_idx) << CACHELINE_PER_WAY_OFFSET_BITS) | __byte_idx)
//#define __comb_cacheline_addr(__tag, __grp_idx) __parse_addr(__tag, __grp_idx, 0)
#define __comb_cacheline_addr(__tag, __grp_idx) (((__tag) << CACHELINE_TAG_OFFSET_BITS) | ((__grp_idx) << CACHELINE_PER_WAY_OFFSET_BITS))
#define __make_addr_cacheline_align(__addr) ((__addr) & (~CACHELINE_BYTES_MASK));

//内存地址 ： Tag + 组号 + 块内字节偏移
//高速缓存行 ： 有效位 + Tag + 块数据



//模拟硬件cache,正常指令访问cache，都是特殊长度的字节访问，例如1,2,4,8等
class Cache {
	//public: //需要私有化，public调试用
private:
	//sizeof(Cacheline) == 72字节
	typedef struct Cacheline {
		//从一个物理地址类型中取某些bit，using phy_addr_t = uint64_t
		phy_addr_t __tag : CACHELINE_TAG_BITS;
		phy_addr_t __count : CACHELINE_WAYS_BITS;
		phy_addr_t __line_valid : 1; //__valid重名...
		phy_addr_t __dirty : 1;
		uint8_t __data[CACHELINE];
	} Cacheline_t;
public:
	static Cache& get_instance() {
		static  Cache instance;
		return instance;
	}

	static Cache& get_ds_instance() {
		static  Cache ds_instance;
		return ds_instance;
	}


public:
	Cache(const Cache&) = delete;
	Cache(Cache&&) = delete;
	Cache& operator=(const Cache&) = delete;
	Cache& operator=(Cache&&) = delete;

private:
	Cache();
	~Cache() = default;

public:
	status read(void* addr, void* buffer, size_t n_bytes,
		eviction_cb_t eviction_cb, bool use_acc_table = false);

	//默认不写回，不用acc table
	status write(void* addr, void* buffer, size_t n_bytes,
		eviction_cb_t eviction_cb, bool write_back = false, bool use_acc_table = false);

	status clear_cache();

private:

	status __read(phy_addr_t addr, addr_t buffer, size_t n_bytes, eviction_cb_t eviction_cb, bool use_acc_table);
	
	status __write(phy_addr_t addr, addr_t buffer, size_t n_bytes,
		eviction_cb_t eviction_cb, bool write_back, bool use_acc_table);

	//刷新缓存
	status hit_fresh(Cacheline_t* cacheline_ways, size_t hit_index);

	//selected_index类似一个寄存器地址，不是访存
	status select_cacheline(Cacheline_t* cacheline_ways, size_t* selected_index);

	status write_acc_table(phy_addr_t* acc_table, size_t idx, phy_addr_t addr);

private:
	Cacheline_t cache[CACHELINES_PER_WAY][CACHELINE_WAYS];
};

/*Cache API*/

status cache_data_move(void* dst, void* src, size_t n_bytes);

status memory_data_read(void* addr, void* buffer, size_t n_bytes = CACHELINE);

status memory_data_write(void* addr, void* buffer, size_t n_bytes = CACHELINE);

status read_cache(void* addr, void* buffer, size_t n_bytes, 
					eviction_cb_t eviction_cb = nullptr, bool use_acc_table = false);

status write_cache(void* addr, void* buffer, size_t n_bytes, 
					eviction_cb_t eviction_cb = nullptr, bool write_back = false, bool use_acc_table = false);

status clear_cache();


#endif