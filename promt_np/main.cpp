#include "mem/promt_np.h"
#include <fstream>
#include <iostream>

using namespace std;
constexpr char READ = 'R';
constexpr char WRITE = 'W';
/*
	统计量：
	1. 总执行时间
	2. 用户数据写次数，加入安全模块导致的额外写次数
	3. 用户数据读次数，加入安全模块导致的额外读次数
	4. Hash计算的次数
	5. Metacache的命中率（统计Metacache的命中次数和缺失次数）
*/

#ifdef PROMT_DEBUG

void UT_hotpage_replace_split(ProMT& ea) {
	int addr = 1;
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	page_addr_t page_addr;
	for (int i = 0; i < 512; i++) {
		page_addr = addr << PAGE_BITS;
		for (int k = 0; k < 4; k++) {
			ea.write(page_addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
		}
		addr++;
	}
}


/*test: hotpage替换*/
void UT_hotpage_replace() {
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	// num带回meta cache的读/写次数，read先返回总延迟
	ProMT ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	int addr = 1;
	page_addr_t page_addr;
	UT_hotpage_replace_split(ea);
	print_sq_size();
	print_hotpage_map();
	page_addr = 120 << PAGE_BITS;
	ea.write(page_addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	print_sq_size();
	print_hotpage_map();
}

void UT_trace_test() {
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	// num带回meta cache的读/写次数，read先返回总延迟
	ProMT ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	ifstream infile;
	infile.open("trace", ios::in);
	if (!infile.is_open()) {
		cout << "failed to open the trace" << endl;
		return;
	}
	Tick clock;
	char rw;
	Addr addr;
	for (int i = 0; i < 10; i++) {
		// ticks
		infile >> clock;
		// RW
		infile >> rw;
		// addr
		infile >> addr;
		if (rw == READ) {
			ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
			cout << mem_writes << ", " << mem_reads << ", " << ex_writes << ", " << ex_reads << ", "
				<< hash << ", " << cache_hit << ", " << cache_miss << endl;
		}
		else if (rw == WRITE) {
			ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
			//cout << mem_writes << ", " << mem_reads << ", " << ex_writes << ", " << ex_reads << ", "
				//<< hash << ", " << cache_hit << ", " << cache_miss << endl;
		}
		else {
			cout << "err" << endl;
		}
	}
	cout << mem_writes << ", " << mem_reads << ", " << ex_writes << ", " << ex_reads << ", "
		<< hash << ", " << cache_hit << ", " << cache_miss << endl;
}

void UT_Multiqueue_test() {
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	// num带回meta cache的读/写次数，read先返回总延迟
	ProMT ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	// 依次写一次
	page_addr_t page_addr = 16;
	phy_addr_t addr;
	for (int i = 0; i < 10; i++) {
		addr = page_addr << PAGE_BITS;
		ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
		page_addr++;
	}
	print_mq();
	// 升级测试
	// 首部
	page_addr = 16;
	addr = page_addr << PAGE_BITS;
	ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	print_mq();
	// 中部
	page_addr = 20;
	addr = page_addr << PAGE_BITS;
	ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	print_mq();
	// 尾部
	page_addr = 0x18;
	addr = page_addr << PAGE_BITS;
	ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	print_mq();
	// 同级队列
	page_addr = 0x14;
	addr = page_addr << PAGE_BITS;
	ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	print_mq();
}

void UT_Hotpage_map_test() {
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	// num带回meta cache的读/写次数，read先返回总延迟
	ProMT ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	// 依次写一次
	page_addr_t page_addr = 16;
	phy_addr_t addr;
	for (int i = 0; i < 512; i++) {
		addr = page_addr << PAGE_BITS;
		for (int k = 0; k < 8; k++) {
			ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
		}
		page_addr++;
	}
	for (int i = 0; i < 8; i++) {
		addr = page_addr << PAGE_BITS;
		for (int k = 0; k < 4; k++) {
			ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
		}
		page_addr++;
	}
	print_sq_size();
	print_hotpage_map();
}

void UT_g_multiqueue_update() {
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	// num带回meta cache的读/写次数，read先返回总延迟
	ProMT ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	// 一次降级，直接出队列
	page_addr_t page_addr = 16;
	phy_addr_t addr = page_addr << PAGE_BITS;
	ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	cout << "memory write cntr: " << g_Statistics.get_total_write_cntr() << endl;

	page_addr++;
	for (int i = 0; i < 98; i++) {
		addr = page_addr << PAGE_BITS;
		ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
		
		page_addr++;
	}
	
	print_sq_size();
	//print_hotpage_map();
}

void UT_latency_test() {
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	// num带回meta cache的读/写次数，read先返回总延迟
	ProMT ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	// 一次降级，直接出队列
	page_addr_t page_addr = 16;
	phy_addr_t addr = page_addr << PAGE_BITS;
	Tick sum = 0;
	sum = ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	cout << "memory write cntr: " << g_Statistics.get_total_write_cntr() << endl;

	page_addr++;
	for (int i = 0; i < 98; i++) {
		addr = page_addr << PAGE_BITS;
		sum += ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);

		page_addr++;
	}
	cout << "write_latency: " << sum << endl;
	//print_sq_size();
	//print_hotpage_map();
}

#endif
int main()
{
	//UT_Multiqueue_test();
	//UT_Hotpage_map_test();
	//UT_g_multiqueue_update();
	UT_latency_test();
	return 0; 
}