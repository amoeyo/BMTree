#include "baseline_CC.h"

using namespace std;

BaseCC::BaseCC(cntr_t& mem_writes, cntr_t& mem_reads, cntr_t& ex_writes, cntr_t& ex_reads, 
		cntr_t& hash, cntr_t& cache_hit, cntr_t& cache_miss):  
mem_writes(mem_writes), mem_reads(mem_reads), ex_writes(ex_writes), ex_reads(ex_reads),
hash(hash), cache_hit(cache_hit), cache_miss(cache_miss)
{
	//cout<< latency_aes << " "<< latency_CACHE_READ << endl;
}

Tick
BaseCC::read(Addr addr, cntr_t& mem_writes, cntr_t& mem_reads, cntr_t& ex_writes, cntr_t& ex_reads,
	cntr_t& hash, cntr_t& cache_hit, cntr_t& cache_miss)
{
	// The whole latency need to inject
	Tick sum = 0;
	//Addr cache_line_id = TO_CLD(pkt->getAddr());
	//printf("cur_addr: %d\n", (int)(pkt->getAddr()));
#ifdef BASELINE
	baseline_read(addr, CACHELINE, sum);
#endif // BASELINE

#ifdef STRICT_PERSIST
	strict_persist_read(addr, CACHELINE, sum);
#endif // STRICT_PERSIST

#ifdef ATOMIC_UPDATE
	atomic_update_read(addr, CACHELINE, sum);
#endif // ATOMIC_UPDATE
	

	mem_writes = g_Statistics.get_user_write_cntr();
	mem_reads = g_Statistics.get_user_read_cntr();
	ex_writes = g_Statistics.get_extra_engine_write_cntr();
	ex_reads = g_Statistics.get_extra_engine_read_cntr();
	hash = g_Statistics.get_hash_cntr();
	cache_hit = g_Statistics.get_cache_hit_cntr();
	cache_miss = g_Statistics.get_cache_miss_cntr();

	// now we get the counter in the cache
	// OTP generation, covered by data read
	//if(latency_aes + cache_read > nvm_read_lat) {
	//	sum += latency_aes + cache_read - nvm_read_lat;
	//}

	return sum;
}

Tick
BaseCC::write(Addr addr, cntr_t& mem_writes, cntr_t& mem_reads, cntr_t& ex_writes, cntr_t& ex_reads,
	cntr_t& hash, cntr_t& cache_hit, cntr_t& cache_miss)
{
	Tick sum = 0;
	uint8_t* buffer = nullptr;
#ifdef BASELINE
	baseline_write(addr, CACHELINE, sum);
#endif // BASELINE

#ifdef STRICT_PERSIST
	strict_persist_write(addr, CACHELINE, sum);
#endif // STRICT_PERSIST

#ifdef ATOMIC_UPDATE
	atomic_update_write(addr, CACHELINE, sum);
#endif // ATOMIC_UPDATE

	// for DMAC, it can be parallel with the OTP generation
	// sum += latency_hmac;
	return sum;
}

