#include "descriptor.h"

ds_addr_t DescriptorBlockIdxTable; //�����ڴˣ���ʼ����MQ����СΪMEMORY_RANGE / PAGE
//DescriptorBlock* DescriptorBlockIdxTable = 0; //��cacheһ���ʼ����ͬacc_table����СΪMEMORY_RANGE / PAGE

DescriptorBlock::DescriptorBlock(page_addr_t addr)
	: __ds_high_bits(0), __ds_low_bits(0) {
	__set_page_addr(addr);
}

DescriptorBlock::DescriptorBlock()
	: __ds_high_bits(0), __ds_low_bits(0) {
	__set_page_addr(0);
}

status 
DescriptorBlock::__set_page_addr(page_addr_t addr) {
	//page_addr_t page_addr = addr >> PAGE_BITS;
	__set_high_bits(DESCRIPTOR_PAGE_ADDR_MASK, DESCRIPTOR_PAGE_ADDR_OFFSET, addr);
	return SUCCESS;
}

status 
DescriptorBlock::set_page_addr(phy_addr_t addr) {
	page_addr_t page_addr = addr >> PAGE_BITS;
	__set_high_bits(DESCRIPTOR_PAGE_ADDR_MASK, DESCRIPTOR_PAGE_ADDR_OFFSET, page_addr);
	return SUCCESS;
}

status 
DescriptorBlock::get_page_addr(page_addr_t* page_addr) {
	__get_value(__ds_high_bits, DESCRIPTOR_PAGE_ADDR_MASK, DESCRIPTOR_PAGE_ADDR_OFFSET, page_addr);
	return SUCCESS;
}

status 
DescriptorBlock::set_acc_num(uint64_t acc_num) {
	__set_high_bits(DESCRIPTOR_PAGE_ACCESS_NUM_MASK, DESCRIPTOR_PAGE_ACCESS_NUM_OFFSET, acc_num);
	return SUCCESS;
}

status 
DescriptorBlock::get_acc_num(uint64_t* acc_num) {
	__get_value(__ds_high_bits, DESCRIPTOR_PAGE_ACCESS_NUM_MASK, DESCRIPTOR_PAGE_ACCESS_NUM_OFFSET, acc_num);
	return SUCCESS;
}

status 
DescriptorBlock::set_exp_time(uint64_t exp_time) {
	__set_high_bits(DESCRIPTOR_PAGE_EXPIRATION_TIME_MASK, 0, exp_time);
	return SUCCESS;
}

status 
DescriptorBlock::get_exp_time(uint64_t* exp_time) {
	__get_value(__ds_high_bits, DESCRIPTOR_PAGE_EXPIRATION_TIME_MASK, 0, exp_time);
	return SUCCESS;
}

status 
DescriptorBlock::set_queue_num(uint64_t queue_num) {
	__set_low_bits(DESCRIPTOR_PAGE_QUEUE_NUM_MASK, DESCRIPTOR_PAGE_QUEUE_NUM_OFFSET, queue_num);
	return SUCCESS;
}

status 
DescriptorBlock::get_queue_num(uint64_t* queue_num) {
	__get_value(__ds_low_bits, DESCRIPTOR_PAGE_QUEUE_NUM_MASK, DESCRIPTOR_PAGE_QUEUE_NUM_OFFSET, queue_num);
	return SUCCESS;
}

status 
DescriptorBlock::set_demo_flag(uint64_t demo_flag) {
	__set_low_bits(DESCRIPTOR_PAGE_DEMOTION_FLAG_MASK, DESCRIPTOR_PAGE_DEMOTION_FLAG_OFFSET, demo_flag);
	return SUCCESS;
}

status 
DescriptorBlock::get_demo_flag(uint64_t* demo_flag) {
	__get_value(__ds_low_bits, DESCRIPTOR_PAGE_DEMOTION_FLAG_MASK, DESCRIPTOR_PAGE_DEMOTION_FLAG_OFFSET, demo_flag);
	return SUCCESS;
}

status 
DescriptorBlock::set_hot_addr(uint64_t hot_addr) {
	__set_low_bits(DESCRIPTOR_PAGE_HOT_ADDR_MASK, DESCRIPTOR_PAGE_HOT_ADDR_OFFSET, hot_addr);
	return SUCCESS;
}

status 
DescriptorBlock::get_hot_addr(uint64_t* hot_addr) {
	__get_value(__ds_low_bits, DESCRIPTOR_PAGE_HOT_ADDR_MASK, DESCRIPTOR_PAGE_HOT_ADDR_OFFSET, hot_addr);
	return SUCCESS;
}

status 
DescriptorBlock::set_frame_pointer(uint64_t next_frame) {
	__set_low_bits(DESCRIPTOR_PAGE_FRAME_POINTER_MASK, 0, next_frame);
	return SUCCESS;
}

status 
DescriptorBlock::get_frame_pointer(uint64_t* next_frame) {
	__get_value(__ds_low_bits, DESCRIPTOR_PAGE_FRAME_POINTER_MASK, 0, next_frame);
	return SUCCESS;
}

status 
DescriptorBlock::__get_value(uint64_t bits, uint64_t mask, uint64_t offset, uint64_t* value) {
	uint64_t temp_value = bits & (mask << offset); //ȡ����Ӧλ��
	temp_value = temp_value >> offset;
	*value = temp_value;
	return SUCCESS;
}

status 
DescriptorBlock::__set_high_bits(uint64_t mask, uint64_t offset, uint64_t value) {
	uint64_t temp_value = value & mask;
	__ds_high_bits &= (~(mask << offset));
	__ds_high_bits |= (temp_value << offset);
	return SUCCESS;
}

status 
DescriptorBlock::__set_low_bits(uint64_t mask, uint64_t offset, uint64_t value) {
	uint64_t temp_value = value & mask;
	__ds_low_bits &= (~(mask << offset));
	__ds_low_bits |= (temp_value << offset);
	return SUCCESS;
}

// ����ҳ��ַ�����λ����
status 
DescriptorBlock::clear() {
	__ds_high_bits &= (DESCRIPTOR_PAGE_ADDR_MASK << DESCRIPTOR_PAGE_ADDR_OFFSET);
	__ds_low_bits = 0;
	return SUCCESS;
}

#ifdef PROMT_DEBUG
// ͨ��DescriptorBlockIdxTable + idx����
status DescriptorBlock::print() {
	uint64_t page_addr, acc_num, exp_time, que_num, demo_flag, hotec, framepointer;
	this->get_page_addr(&page_addr);
	this->get_acc_num(&acc_num);
	this->get_exp_time(&exp_time);
	this->get_queue_num(&que_num);
	this->get_demo_flag(&demo_flag);
	this->get_hot_addr(&hotec);
	this->get_frame_pointer(&framepointer);
	fmt_print("-----------------------------");
	fmt_print_k_hex_v("| page: ", page_addr);
	fmt_print_k_hex_v("| access number: ", acc_num);
	fmt_print_k_hex_v("| expiration time: ", exp_time);
	fmt_print_k_hex_v("| queue number: ", que_num);
	fmt_print_k_hex_v("| demotion flag: ", demo_flag);
	fmt_print_k_hex_v("| padding/hot EC: ", hotec);
	fmt_print_k_hex_v("| next ds: ", framepointer);
	fmt_print("-----------------------------");
	return SUCCESS;
}
#endif

/*SQ*/

SingleQueue::SingleQueue(uint64_t idx)
	: __queue_idx(idx), __ds_queue_head(0), __size(0) {
	__max_rw_num = (1 << (idx + 1)) - 1;
}

SingleQueue::SingleQueue()
	: __queue_idx(0), __ds_queue_head(0), __size(0), __max_rw_num(0) {

}

// ���ڽ�ds_pageֱ�ӹҵ���β����ʱ������û��ds_page������Ҫ����ds_page�Ƿ����
status 
SingleQueue::push_back(page_addr_t ds_page) {
	// ֱ�ӹҵ�����β�Ĳ�����ֻ�漰���һ��Ԫ�ص��޸�
	ds_blk_t ds_register; //�Ĵ���
	page_addr_t next_frame;
	ds_addr_t cur_ds;
	cur_ds = &__ds_queue_head;
	cur_ds->get_frame_pointer(&next_frame);
	if (next_frame == 0) { //����Ϊ��
		__ds_queue_head.set_frame_pointer(ds_page);
		__size++;
		return PUSH_BACK_SUCCESS;
	}
	while (next_frame != 0) {
		//��������ds_table[next_frame]�����ܻ�ô�
		cur_ds = DescriptorBlockIdxTable + next_frame;//�����ַ
		read_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));//�����浽�Ĵ���
		ds_register.get_frame_pointer(&next_frame);
	}
	// �˳�ѭ��˵���ҵ������һ��ds
	ds_register.set_frame_pointer(ds_page);
	// �޸�֮��д���棬��д��
	write_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));
	__size++;
	return PUSH_BACK_SUCCESS;
}

// ���ض�Ԫ�ؿ�ʼ�ҵ�����β��������ds�����ڶ����У���Ҫ�ƶ�������β�����
// ds_head��һ��ָ��
status 
SingleQueue::__push_back(ds_addr_t ds_head, page_addr_t ds_page) {
	//page_addr_t next_frame;
	//ds_addr_t cur_ds;
	//ds_blk_t ds_register;
	//cur_ds = ds_head;
	//read_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));//�����浽�Ĵ���
	//ds_register.get_frame_pointer(&next_frame);
	//while (next_frame != 0) {
	//	//��������ds_table[next_frame]�����ܻ�ô�
	//	cur_ds = DescriptorBlockIdxTable + next_frame;//�����ַ
	//	read_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));//�����浽�Ĵ���
	//	//cur_ds->get_frame_pointer(&next_frame);
	//	ds_register.get_frame_pointer(&next_frame);
	//}
	//// �˳�ѭ��˵���ҵ������һ��ds
	//ds_register.set_frame_pointer(ds_page);
	//// �޸�֮��д����
	//write_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));
	////cur_ds->set_frame_pointer(ds_page);
	////����push����ĩӦ�ò���Ҫд���棬�����ⲿȥ����next_frame�������0
	////ds->set_frame_pointer(0);
	////write_ds_cache(ds, &ds_register, sizeof(DescriptorBlock));
	return PUSH_BACK_SUCCESS;
}

// ds��һ���Ĵ���
// ����У������漰��һ��DS��β��DS�ͱ�DSָ���޸�
status 
SingleQueue::push_back_n(ds_addr_t ds, page_addr_t ds_page) {
	page_addr_t next_frame;
	ds_addr_t cur_ds;
	ds_blk_t ds_register;
	cur_ds = &__ds_queue_head;
	cur_ds->get_frame_pointer(&next_frame);
	if (next_frame == 0) { //�׽ڵ�
		__ds_queue_head.set_frame_pointer(ds_page);
		__size++;
		return PUSH_BACK_SUCCESS;
	}
	if (next_frame == ds_page) { //�漰�׽ڵ�ĸ���Ҫ��������
		ds->get_frame_pointer(&next_frame);
		__ds_queue_head.set_frame_pointer(next_frame);
		__size--;
		uint64_t cur_acc_num;
		ds->get_acc_num(&cur_acc_num);
		if (cur_acc_num > __max_rw_num) {
			//push_back����һ��Ķ���
			return PUSH_NEXT_LEVEL;
		}
		else {
			return PUSH_BACK_THIS;
		}
	}

	while (next_frame != ds_page && next_frame != 0) { //����Ӧ�ÿ϶�������	
		//��������ds_table[next_frame]�����ܻ�ô�
		cur_ds = DescriptorBlockIdxTable + next_frame;
		read_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));
		ds_register.get_frame_pointer(&next_frame);
	}
	if (next_frame == 0) {
		//û���ţ�ֱ�ӹҵ���β���޸����һ��Ԫ�ص�ָ��
		ds_register.set_frame_pointer(ds_page);
		write_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));
		__size++;
		return PUSH_BACK_SUCCESS;
	}
	else {
		//�����ˣ��޸��������ds��ָ��ָ����ds��next_frame
		ds->get_frame_pointer(&next_frame);
		ds_register.set_frame_pointer(next_frame);
		// �޸�֮��Ҫ�ǵ�д����
		write_ds_cache(cur_ds, &ds_register, sizeof(ds_blk_t));
		ds->set_frame_pointer(0);
		// ���access counter��û�г����ޣ����û�����ͶԵ�ǰ����push_back�������˾�push_back����һ��
		uint64_t cur_acc_num;
		ds->get_acc_num(&cur_acc_num);
		if (cur_acc_num > __max_rw_num) {
			//push_back����һ��Ķ���
			__size--;
			return PUSH_NEXT_LEVEL;
		}
		else {
			__size--;
			return PUSH_BACK_THIS;
		}
	}
}

// �������ͷ��������
status 
SingleQueue::evict_head(ds_addr_t ds_reg) {
	page_addr_t page_addr;
	ds_blk_t ds_register(0);
	__ds_queue_head.get_frame_pointer(&page_addr);
	ds_addr_t head_ds = DescriptorBlockIdxTable + page_addr;
	// ��ȡ��ǰ���е�һ��ds
	read_ds_cache(head_ds, ds_reg, sizeof(ds_blk_t));
	// ��ȡ��ǰdsָ�����һ��ds�������ͷ����ds_reg����
	ds_reg->get_frame_pointer(&page_addr);
	// ͷ�ڵ�ָ��ı�
	__ds_queue_head.set_frame_pointer(page_addr);
	// ��ҳ��ַ�����λ���㣬д�ػ���
	ds_register.__set_page_addr(page_addr);
	/*д���棬����֪��Ҫ��Ҫд��*/
	write_ds_cache(head_ds, &ds_register, sizeof(ds_blk_t));
	__size--;
	return SUCCESS;
}


status 
SingleQueue::get_head(ds_addr_t ds_reg) {
	page_addr_t page_addr;
	ds_blk_t ds_register(0);
	__ds_queue_head.get_frame_pointer(&page_addr);
	ds_addr_t head_ds = DescriptorBlockIdxTable + page_addr;
	// ��ȡ��ǰ���е�һ��ds
	read_ds_cache(head_ds, ds_reg, sizeof(ds_blk_t));

	return SUCCESS;
}

status 
SingleQueue::set_queue_idx(uint64_t idx) {
	__queue_idx = idx;
	__max_rw_num = (1 << (idx + 1)) - 1;
	return SUCCESS;
}

uint64_t 
SingleQueue::get_queue_size() {
	return __size;
}

#ifdef PROMT_DEBUG
status SingleQueue::print_sq() {
	fmt_print_kv("queue ", __queue_idx);
	fmt_print_kv("size: ", __size);
	fmt_print_kv("max acc num: ", __max_rw_num);
	fmt_print("======================");
	page_addr_t next_page;
	__ds_queue_head.get_frame_pointer(&next_page);
	ds_blk_t ds_reg;
	while (next_page != 0) {
		ds_addr_t cur_ds = DescriptorBlockIdxTable + next_page;
		read_ds_cache(cur_ds, &ds_reg, sizeof(ds_blk_t));
		ds_reg.print();
		ds_reg.get_frame_pointer(&next_page);
	}
	fmt_print("======================");
	return SUCCESS;
}

status SingleQueue::print_status() {
	fmt_print_kv("queue ", __queue_idx);
	fmt_print_kv("size: ", __size);
	fmt_print("======================");
	page_addr_t next_page;
	__ds_queue_head.get_frame_pointer(&next_page);
	ds_blk_t ds_reg;
	if (next_page != 0) {
		ds_addr_t cur_ds = DescriptorBlockIdxTable + next_page;
		read_ds_cache(cur_ds, &ds_reg, sizeof(ds_blk_t));
		ds_reg.print();
		ds_reg.get_frame_pointer(&next_page);
	}
	fmt_print("======================");
	return SUCCESS;
}
#endif // PROMT_DEBUG


/*MQ*/

MultiQueue::MultiQueue() {
	for (int i = 0; i < MULTIQUEUE_SIZE; i++) {
		queue_set[i].set_queue_idx(i);
	}
	// ��ʼ��ds����ÿ��page����Ӧһ��ds
	// sizeof(ds_blk_t) = 16
	DescriptorBlockIdxTable = (ds_addr_t)memory_alloc((MEMORY_RANGE / PAGE) * sizeof(ds_blk_t), PAGE);
	memset(DescriptorBlockIdxTable, 0, (MEMORY_RANGE / PAGE) * sizeof(ds_blk_t));
	// ��ʼ��ÿ��DS���ҳ��ַ
	ds_addr_t p = DescriptorBlockIdxTable;
	for (int i = 0; i < MEMORY_RANGE / PAGE; i++) {
		(p + i)->__set_page_addr((page_addr_t)i);
	}
}

// ��ds�ӵ�queue[idx]���У�ds��һ���Ĵ������ڹؼ�·���ϴ�cache���������
status 
MultiQueue::push_queue_idx(ds_addr_t ds, uint64_t* idx) {
	uint64_t page_addr;
	ds->get_page_addr(&page_addr);
	status push_stat = queue_set[*idx].push_back_n(ds, page_addr);
	if (push_stat == PUSH_NEXT_LEVEL) {
		(*idx)++;
		if (*idx < MULTIQUEUE_SIZE) {
			queue_set[*idx].push_back(page_addr);
		}
		else {
			(*idx)--;
			queue_set[*idx].push_back(page_addr);
		}
		
	}
	else if (push_stat == PUSH_BACK_THIS) {
		queue_set[*idx].push_back(page_addr);
	}
	return SUCCESS;
}
// ��dsֱ�Ӽӵ�queue[idx]��β���޸Ķ�βԪ�ص�frame pointer��д�أ�����������ͷ����������
status 
MultiQueue::push_back_queue_idx(ds_addr_t ds, uint64_t idx) {
	uint64_t page_addr;
	ds->get_page_addr(&page_addr);
	queue_set[idx].push_back(page_addr);
	return SUCCESS;
}

status 
MultiQueue::evict_queue_head(uint64_t idx, ds_addr_t ds_reg) {
	return queue_set[idx].evict_head(ds_reg);
}

status 
MultiQueue::get_queue_head(uint64_t idx, ds_addr_t ds_reg) {
	return queue_set[idx].get_head(ds_reg);
}

uint64_t 
MultiQueue::get_queue_idx_size(uint64_t idx) {
	return queue_set[idx].get_queue_size();
}

#ifdef PROMT_DEBUG
status MultiQueue::print_mq() {
	for (int i = 0; i < MULTIQUEUE_SIZE; i++) {
		queue_set[i].print_sq();
	}
	return SUCCESS;
}

status MultiQueue::print_sq_size() {
	for (int i = 0; i < MULTIQUEUE_SIZE; i++) {
		queue_set[i].print_status();
	}
	return SUCCESS;
}
#endif

MultiQueue& g_Mutiqueue = MultiQueue::get_instance();//static, ͬcache

status
push_queue_idx(ds_addr_t ds, uint64_t* idx) {
	return g_Mutiqueue.push_queue_idx(ds, idx);
}

status
push_back_queue_idx(ds_addr_t ds, uint64_t idx) {
	return g_Mutiqueue.push_back_queue_idx(ds, idx);
}

status
evict_queue_head(uint64_t idx, ds_addr_t ds_reg) {
	return g_Mutiqueue.evict_queue_head(idx, ds_reg);
}

status
get_queue_head(uint64_t idx, ds_addr_t ds_reg) {
	return g_Mutiqueue.get_queue_head(idx, ds_reg);
}

uint64_t
get_queue_idx_size(uint64_t idx) {
	return g_Mutiqueue.get_queue_idx_size(idx);
}

#ifdef PROMT_DEBUG
status
print_mq() {
	return g_Mutiqueue.print_mq();
}

status
print_sq_size() {
	return g_Mutiqueue.print_sq_size();
}

#endif


