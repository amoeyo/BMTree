#include "mem/baseline_CC.h"
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

/**
//检查缓存被动刷回时extra write计数问题
void UT_cache_test()
{
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	// num带回meta cache的读/写次数，read先返回总延迟
	BaseCC ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	
	uint64_t addr_group = 0x3;
	uint64_t addr_tag = 0x4;
	uint64_t addr = __comb_cacheline_addr(addr_tag, addr_group);
	for (int i = 0; i < CACHELINE_WAYS; i++) {
		ea.read(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
		cout << mem_writes << ", " << mem_reads << ", " << ex_writes << ", " << ex_reads << ", "
			<< hash << ", " << cache_hit << ", " << cache_miss << endl;
		addr_tag++;
		addr = __comb_cacheline_addr(addr_tag, addr_group);
	}
	// 写满
	// 再写一个，此时这个缓存行变成了dirty
	uint64_t victim_line = 0x5;
	uint64_t victim_addr = __comb_cacheline_addr(victim_line, addr_group);
	ea.write(victim_addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	cout << mem_writes << ", " << mem_reads << ", " << ex_writes << ", " << ex_reads << ", "
		<< hash << ", " << cache_hit << ", " << cache_miss << endl;
	// 读一个新的触发被动替换
	ea.read(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	cout << mem_writes << ", " << mem_reads << ", " << ex_writes << ", " << ex_reads << ", "
		<< hash << ", " << cache_hit << ", " << cache_miss << endl;
}
**/

int main()
{
	//cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	//// num带回meta cache的读/写次数，read先返回总延迟
	//BaseCC ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	//
	//ifstream infile;
	//infile.open("trace", ios::in);
	//if (!infile.is_open()) {
	//	cout << "failed to open the trace" << endl;
	//	return 0;
	//}
	//Tick clock;
	//char rw;
	//Addr addr;
	//for (int i = 0; i < 3000; i++) {
	//	// ticks
	//	infile >> clock;
	//	// RW
	//	infile >> rw;
	//	// addr
	//	infile >> addr;
	//	if (rw == READ) {
	//		ea.read(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	//		cout << rw << ", " << mem_writes << ", " << mem_reads << ", " << ex_writes << ", " << ex_reads << ", "
	//			<< hash << ", " << cache_hit << ", " << cache_miss << endl;
	//	}
	//	else if (rw == WRITE) {
	//		ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	//		cout << rw << ", " << mem_writes << ", " << mem_reads << ", " << ex_writes << ", " << ex_reads << ", "
	//			<< hash << ", " << cache_hit << ", " << cache_miss << endl;
	//	}
	//	else {
	//		cout << "err" << endl;
	//	}
	//}
	//UT_cache_test();
	return 0;
}