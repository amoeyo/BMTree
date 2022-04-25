#ifndef __DESCRIPTOR_HH__
#define __DESCRIPTOR_HH__

#include "common.h"
#include "cache.h"
#include "ds_cache.h"

#define TAIL_POINTER_OPTI

//Descriptor���
constexpr size_t DESCRIPTOR_RANGE = MEMORY_RANGE / PAGE; //DS������ÿ��PAGE��Ӧһ��DS
constexpr size_t DESCRIPTOR_BITS = 128; //128bit
constexpr size_t DESCRIPTOR_PAGE_ADDR_BITS = 36;	//page addr
constexpr size_t DESCRIPTOR_PAGE_ACCESS_NUM_BITS = 8;	//PAGE��д�Ĵ���
constexpr size_t DESCRIPTOR_PAGE_QUEUE_NUM = 3;	//�����ĸ�queue
constexpr size_t DESCRIPTOR_PAGE_DEMOTION_FLAG = 1;	//�Ƿ��Ѿ�������һ��
constexpr size_t DESCRIPTOR_PAGE_EXPIRATION_TIME = 20;	//���ʱ��
constexpr size_t DESCRIPTOR_PAGE_HOT_ADDR = 36;	//hot tree index or cold
constexpr size_t DESCRIPTOR_PAGE_FRAME_POINTER = 24; //ָ����е���һ��DS

constexpr size_t DESCRIPTOR_PAGE_ADDR_OFFSET = 28;
constexpr size_t DESCRIPTOR_PAGE_ACCESS_NUM_OFFSET = 20;
constexpr size_t DESCRIPTOR_PAGE_QUEUE_NUM_OFFSET = 61;
constexpr size_t DESCRIPTOR_PAGE_DEMOTION_FLAG_OFFSET = 60;
constexpr size_t DESCRIPTOR_PAGE_HOT_ADDR_OFFSET = 24;

constexpr uint64_t DESCRIPTOR_PAGE_ADDR_MASK = MAKE_MASK(DESCRIPTOR_PAGE_ADDR_BITS, uint64_t);
constexpr uint64_t DESCRIPTOR_PAGE_ACCESS_NUM_MASK = MAKE_MASK(DESCRIPTOR_PAGE_ACCESS_NUM_BITS, uint64_t);
constexpr uint64_t DESCRIPTOR_PAGE_EXPIRATION_TIME_MASK = MAKE_MASK(DESCRIPTOR_PAGE_EXPIRATION_TIME, uint64_t);
constexpr uint64_t DESCRIPTOR_PAGE_QUEUE_NUM_MASK = MAKE_MASK(DESCRIPTOR_PAGE_QUEUE_NUM, uint64_t);
constexpr uint64_t DESCRIPTOR_PAGE_DEMOTION_FLAG_MASK = MAKE_MASK(DESCRIPTOR_PAGE_DEMOTION_FLAG, uint64_t);
constexpr uint64_t DESCRIPTOR_PAGE_HOT_ADDR_MASK = MAKE_MASK(DESCRIPTOR_PAGE_HOT_ADDR, uint64_t);
constexpr uint64_t DESCRIPTOR_PAGE_FRAME_POINTER_MASK = MAKE_MASK(DESCRIPTOR_PAGE_FRAME_POINTER, uint64_t);

constexpr uint64_t NULL_TAIL = 0xffffff;

constexpr uint64_t MULTIQUEUE_SIZE = 8;

constexpr status PUSH_NEXT_LEVEL = 0;
constexpr status PUSH_BACK_SUCCESS = 1;
constexpr status PUSH_BACK_THIS = 3;

//Descriptor Structure 128bit
//��Cacheline����

/*
*	������һ��˳�򣬸պ��ֳܷ�����64b
	|----------------high--------------------------|-------------------------low-------------------------|
	|---------36b--------|-----8b----|-----20b-----|----3b----|---1b---|--------36b-------|------24b-----|
	|-------PageNum------|-AccessNum-|--ExpirTime--|--QueNum--|DemoFlag|--Padding/HotEC---|-FramePointer-|
*/
class DescriptorBlock {
public:
	//��ʼ����ÿ��ҳ����һ��DS������ҳ��ַ������λ������
	DescriptorBlock(page_addr_t addr);
	DescriptorBlock();

public:
	//get/set special bits
	status __set_page_addr(page_addr_t addr);
	status set_page_addr(phy_addr_t addr);
	status get_page_addr(page_addr_t* page_addr);

	status set_acc_num(uint64_t acc_num);
	status get_acc_num(uint64_t* acc_num);

	status set_exp_time(uint64_t exp_time);
	status get_exp_time(uint64_t* exp_time);

	status set_queue_num(uint64_t queue_num);
	status get_queue_num(uint64_t* queue_num);

	status set_demo_flag(uint64_t demo_flag);
	status get_demo_flag(uint64_t* demo_flag);

	status set_hot_addr(uint64_t hot_addr);
	status get_hot_addr(uint64_t* hot_addr);

	status set_frame_pointer(uint64_t next_frame);
	status get_frame_pointer(uint64_t* next_frame);

	status __get_value(uint64_t bits, uint64_t mask, uint64_t offset, uint64_t* value);
	status __set_high_bits(uint64_t mask, uint64_t offset, uint64_t value);
	status __set_low_bits(uint64_t mask, uint64_t offset, uint64_t value);


	// ����ҳ��ַ�����λ����
	status clear();

#ifdef PROMT_DEBUG
	status print();
	status print_status();
#endif

//private:
	uint64_t __ds_high_bits;
	uint64_t __ds_low_bits;
};

typedef DescriptorBlock* ds_addr_t;
typedef DescriptorBlock ds_blk_t;



//����Ӧ��ֻ����һ������ͷ�Ϳ����ˣ�ʣ�µ�ȥcache������ȥ
class SingleQueue {
public:
	SingleQueue(uint64_t idx);

	SingleQueue();

public:
	SingleQueue(const SingleQueue&) = delete;
	SingleQueue(SingleQueue&&) = delete;
	SingleQueue& operator=(const SingleQueue&) = delete;
	SingleQueue& operator=(SingleQueue&&) = delete;

public:
	// ���ڽ�ds_pageֱ�ӹҵ���β����ʱ������û��ds_page������Ҫ����ds_page�Ƿ����
	status push_back(page_addr_t ds_page);

	// ���ض�Ԫ�ؿ�ʼ�ҵ�����β��������ds�����ڶ����У���Ҫ�ƶ�������β�����
	// ds_head��һ��ָ��
	status __push_back(ds_addr_t ds_head, page_addr_t ds_page);

	// ds��һ���Ĵ���
	// ����У������漰��һ��DS��β��DS�ͱ�DSָ���޸�
	status push_back_n(ds_addr_t ds, page_addr_t ds_page);

	// �������ͷ��������
	status evict_head(ds_addr_t ds_reg);

	status get_head(ds_addr_t ds_reg);

	status set_queue_idx(uint64_t idx);

	uint64_t get_queue_size();

	

#ifdef PROMT_DEBUG
	status check_size();
	status print_sq();
	status print_status();
#endif

private:
	uint64_t __queue_idx;
	ds_blk_t __ds_queue_head;//����ֻ����һ��ͷ
#ifdef TAIL_POINTER_OPTI
	ds_blk_t __ds_queue_tail;//����һ��ָ��β����ָ��
#endif
	uint64_t __size;//���д�С
	uint64_t __max_rw_num;
};

class MultiQueue {
public:
	//��ʼ��idx����idx�����нṹ
	MultiQueue();

	~MultiQueue() = default;


public:
	MultiQueue(const MultiQueue&) = delete;
	MultiQueue(MultiQueue&&) = delete;
	MultiQueue& operator=(const MultiQueue&) = delete;
	MultiQueue& operator=(MultiQueue&&) = delete;


	//get_instance ͬcacheline��ȫ��ֻ��һ��
	static MultiQueue& get_instance() {
		static MultiQueue instance;
		return instance;
	}

	// ��ds�ӵ�queue[idx]���У�ds��һ���Ĵ������ڹؼ�·���ϴ�cache���������
	status push_queue_idx(ds_addr_t ds, uint64_t* idx);

	// ��dsֱ�Ӽӵ�queue[idx]��β���޸Ķ�βԪ�ص�frame pointer��д�أ�����������ͷ����������
	status push_back_queue_idx(ds_addr_t ds, uint64_t idx);

	status evict_queue_head(uint64_t idx, ds_addr_t ds_reg);

	status get_queue_head(uint64_t idx, ds_addr_t ds_reg);

	uint64_t get_queue_idx_size(uint64_t idx);

#ifdef PROMT_DEBUG
	status print_mq();
	status print_sq_size();
	status check_sq_size();
	status print_sq_idx(uint64_t idx);
#endif

private:
	SingleQueue queue_set[MULTIQUEUE_SIZE];
};

status push_queue_idx(ds_addr_t ds, uint64_t* idx);

status push_back_queue_idx(ds_addr_t ds, uint64_t idx);

status evict_queue_head(uint64_t idx, ds_addr_t ds_reg);

status get_queue_head(uint64_t idx, ds_addr_t ds_reg);

uint64_t get_queue_idx_size(uint64_t idx);

#ifdef PROMT_DEBUG
status print_mq();

status print_sq_size();

status check_sq_size();

status
print_sq_idx(uint64_t idx);
#endif
extern ds_addr_t DescriptorBlockIdxTable; //�����ڴˣ���ʼ����MQ����СΪMEMORY_RANGE / PAGE

#endif