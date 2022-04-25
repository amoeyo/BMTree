#include "mem/baseline_CC.h"
#include <fstream>
#include <iostream>

using namespace std;
constexpr char READ = 'R';
constexpr char WRITE = 'W';
/*
	ͳ������
	1. ��ִ��ʱ��
	2. �û�����д���������밲ȫģ�鵼�µĶ���д����
	3. �û����ݶ����������밲ȫģ�鵼�µĶ��������
	4. Hash����Ĵ���
	5. Metacache�������ʣ�ͳ��Metacache�����д�����ȱʧ������
*/

/**
//��黺�汻��ˢ��ʱextra write��������
void UT_cache_test()
{
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	// num����meta cache�Ķ�/д������read�ȷ������ӳ�
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
	// д��
	// ��дһ������ʱ��������б����dirty
	uint64_t victim_line = 0x5;
	uint64_t victim_addr = __comb_cacheline_addr(victim_line, addr_group);
	ea.write(victim_addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	cout << mem_writes << ", " << mem_reads << ", " << ex_writes << ", " << ex_reads << ", "
		<< hash << ", " << cache_hit << ", " << cache_miss << endl;
	// ��һ���µĴ��������滻
	ea.read(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	cout << mem_writes << ", " << mem_reads << ", " << ex_writes << ", " << ex_reads << ", "
		<< hash << ", " << cache_hit << ", " << cache_miss << endl;
}
**/

int main()
{
	//cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	//// num����meta cache�Ķ�/д������read�ȷ������ӳ�
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