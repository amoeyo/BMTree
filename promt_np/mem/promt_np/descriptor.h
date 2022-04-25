#ifndef __DESCRIPTOR_HH__
#define __DESCRIPTOR_HH__

#include "common.h"
#include "cache.h"
#include "ds_cache.h"

#define TAIL_POINTER_OPTI

//Descriptor相关
constexpr size_t DESCRIPTOR_RANGE = MEMORY_RANGE / PAGE; //DS数量，每个PAGE对应一个DS
constexpr size_t DESCRIPTOR_BITS = 128; //128bit
constexpr size_t DESCRIPTOR_PAGE_ADDR_BITS = 36;	//page addr
constexpr size_t DESCRIPTOR_PAGE_ACCESS_NUM_BITS = 8;	//PAGE被写的次数
constexpr size_t DESCRIPTOR_PAGE_QUEUE_NUM = 3;	//属于哪个queue
constexpr size_t DESCRIPTOR_PAGE_DEMOTION_FLAG = 1;	//是否已经降队列一次
constexpr size_t DESCRIPTOR_PAGE_EXPIRATION_TIME = 20;	//存活时间
constexpr size_t DESCRIPTOR_PAGE_HOT_ADDR = 36;	//hot tree index or cold
constexpr size_t DESCRIPTOR_PAGE_FRAME_POINTER = 24; //指向队列的下一个DS

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
//以Cacheline对齐

/*
*	调整了一下顺序，刚好能分成两半64b
	|----------------high--------------------------|-------------------------low-------------------------|
	|---------36b--------|-----8b----|-----20b-----|----3b----|---1b---|--------36b-------|------24b-----|
	|-------PageNum------|-AccessNum-|--ExpirTime--|--QueNum--|DemoFlag|--Padding/HotEC---|-FramePointer-|
*/
class DescriptorBlock {
public:
	//初始化，每个页都有一个DS，除了页地址外其它位置清零
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


	// 除了页地址以外的位清零
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



//这里应该只保存一个队列头就可以了，剩下的去cache里面找去
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
	// 用于将ds_page直接挂到队尾，此时队列中没有ds_page，不需要搜索ds_page是否存在
	status push_back(page_addr_t ds_page);

	// 从特定元素开始找到队列尾，适用于ds存在于队列中，需要移动到队列尾的情况
	// ds_head是一个指针
	status __push_back(ds_addr_t ds_head, page_addr_t ds_page);

	// ds是一个寄存器
	// 入队列，可能涉及上一个DS，尾部DS和本DS指针修改
	status push_back_n(ds_addr_t ds, page_addr_t ds_page);

	// 驱逐队列头部并带回
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
	ds_blk_t __ds_queue_head;//这里只保存一个头
#ifdef TAIL_POINTER_OPTI
	ds_blk_t __ds_queue_tail;//这是一个指向尾部的指针
#endif
	uint64_t __size;//队列大小
	uint64_t __max_rw_num;
};

class MultiQueue {
public:
	//初始化idx分配idx个队列结构
	MultiQueue();

	~MultiQueue() = default;


public:
	MultiQueue(const MultiQueue&) = delete;
	MultiQueue(MultiQueue&&) = delete;
	MultiQueue& operator=(const MultiQueue&) = delete;
	MultiQueue& operator=(MultiQueue&&) = delete;


	//get_instance 同cacheline，全局只有一个
	static MultiQueue& get_instance() {
		static MultiQueue instance;
		return instance;
	}

	// 把ds加到queue[idx]队列，ds是一个寄存器，在关键路径上从cache里读出来的
	status push_queue_idx(ds_addr_t ds, uint64_t* idx);

	// 把ds直接加到queue[idx]队尾，修改队尾元素的frame pointer并写回，这里适用于头部驱逐的情况
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
extern ds_addr_t DescriptorBlockIdxTable; //声明在此，初始化随MQ，大小为MEMORY_RANGE / PAGE

#endif