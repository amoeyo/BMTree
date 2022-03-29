#include "mem/promt_np.h"
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


/*test: hotpage�滻*/
void UT_hotpage_replace() {
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	// num����meta cache�Ķ�/д������read�ȷ������ӳ�
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
	// num����meta cache�Ķ�/д������read�ȷ������ӳ�
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
	// num����meta cache�Ķ�/д������read�ȷ������ӳ�
	ProMT ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	// ����дһ��
	page_addr_t page_addr = 16;
	phy_addr_t addr;
	for (int i = 0; i < 10; i++) {
		addr = page_addr << PAGE_BITS;
		ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
		page_addr++;
	}
	print_mq();
	// ��������
	// �ײ�
	page_addr = 16;
	addr = page_addr << PAGE_BITS;
	ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	print_mq();
	// �в�
	page_addr = 20;
	addr = page_addr << PAGE_BITS;
	ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	print_mq();
	// β��
	page_addr = 0x18;
	addr = page_addr << PAGE_BITS;
	ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	print_mq();
	// ͬ������
	page_addr = 0x14;
	addr = page_addr << PAGE_BITS;
	ea.write(addr, mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	print_mq();
}

void UT_Hotpage_map_test() {
	cntr_t mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss;
	// num����meta cache�Ķ�/д������read�ȷ������ӳ�
	ProMT ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	// ����дһ��
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
	// num����meta cache�Ķ�/д������read�ȷ������ӳ�
	ProMT ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	// һ�ν�����ֱ�ӳ�����
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
	// num����meta cache�Ķ�/д������read�ȷ������ӳ�
	ProMT ea(mem_writes, mem_reads, ex_writes, ex_reads, hash, cache_hit, cache_miss);
	// һ�ν�����ֱ�ӳ�����
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