#ifndef __GLOBAL_CACHE_HH__
#define __GLOBAL_CACHE_HH__

#include "common.h"
#include "statistics.h"

constexpr size_t ADDR_LEN = (sizeof(phy_addr_t) << 3); //64
constexpr size_t CACHELINE_RANGE = 1024;//1024��cacheline,����ռ�ռ�Լ72KB����
constexpr size_t CACHELINES_PER_WAY = PAGE / CACHELINE;//64��GROUP
constexpr size_t CACHELINE_WAYS = CACHELINE_RANGE / CACHELINES_PER_WAY; //16·������
constexpr size_t CACHELINE_WAYS_MAX = CACHELINE_WAYS - 1;
constexpr size_t CACHELINE_WAYS_BITS = 4;//����bit���Լ���WAYS,��Ҫ��CACHELINE_WAYSͬ������
//constexpr size_t CACHELINE_BITS = 6;//����bit��������Cacheline�ڲ�����ĳ���ֽ�,ƫ��Ϊ0
constexpr size_t CACHELINE_PER_WAY_BITS = 6;//����bit�����������,��6��12λ������CACHELINE_OFFSET_BITS��ʼ��6λ
constexpr size_t CACHELINE_TAG_BITS = (ADDR_LEN - CACHELINE_PER_WAY_BITS - CACHELINE_BITS); //Tagռ��bit��
constexpr size_t CACHELINE_PER_WAY_OFFSET_BITS = CACHELINE_BITS; //group����ƫ��6λ
constexpr size_t CACHELINE_TAG_OFFSET_BITS = (CACHELINE_PER_WAY_OFFSET_BITS + CACHELINE_PER_WAY_BITS); //Tag����ƫ��12λ

constexpr phy_addr_t GROUP_MASK = MAKE_MASK(CACHELINE_PER_WAY_BITS, phy_addr_t); //0x3f
constexpr phy_addr_t CACHELINE_BYTES_MASK = MAKE_MASK(CACHELINE_BITS, phy_addr_t); //0x3f

constexpr status PARAMETER_ERROR = 30;
constexpr status CACHE_HIT = 31;
constexpr status CACHE_MISS = 32;

extern uint8_t* cacheline_register; //64B�����Ĵ���
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

//�ڴ��ַ �� Tag + ��� + �����ֽ�ƫ��
//���ٻ����� �� ��Чλ + Tag + ������



//ģ��Ӳ��cache,����ָ�����cache���������ⳤ�ȵ��ֽڷ��ʣ�����1,2,4,8��
class Cache {
	//public: //��Ҫ˽�л���public������
private:
	//sizeof(Cacheline) == 72�ֽ�
	typedef struct Cacheline {
		//��һ�������ַ������ȡĳЩbit��using phy_addr_t = uint64_t
		phy_addr_t __tag : CACHELINE_TAG_BITS;
		phy_addr_t __count : CACHELINE_WAYS_BITS;
		phy_addr_t __line_valid : 1; //__valid����...
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

	//Ĭ�ϲ�д�أ�����acc table
	status write(void* addr, void* buffer, size_t n_bytes,
		eviction_cb_t eviction_cb, bool write_back = false, bool use_acc_table = false);

	status clear_cache();

private:

	status __read(phy_addr_t addr, addr_t buffer, size_t n_bytes, eviction_cb_t eviction_cb, bool use_acc_table);
	
	status __write(phy_addr_t addr, addr_t buffer, size_t n_bytes,
		eviction_cb_t eviction_cb, bool write_back, bool use_acc_table);

	//ˢ�»���
	status hit_fresh(Cacheline_t* cacheline_ways, size_t hit_index);

	//selected_index����һ���Ĵ�����ַ�����Ƿô�
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