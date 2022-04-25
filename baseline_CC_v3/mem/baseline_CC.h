#ifndef __BASELINE_CC_HH__
#define __BASELINE_CC_HH__

//#include "sim/sim_object.hh"
//#include "mem/abstract_mem.hh"

#include <iostream>
/* lru cache */
#include <unordered_map>
#include <list>
#include <cstddef>
#include <stdexcept>
#include <cstdint>
#include <cmath>
#include "baseline_CC_v2/baseline_rw.h"

#define BASELINE

//#define STRICT_PERSIST

//#define ATOMIC_UPDATE


using Addr = uint64_t;
using Tick = uint64_t;


/*
    Baseline
    tree type:
    update way: w/o CC
    arity:      9
    tips:
*/

// Baseline_CC
class BaseCC
{
public:
    BaseCC(cntr_t& mem_writes, cntr_t& mem_reads, cntr_t& ex_writes, cntr_t& ex_reads,
        cntr_t& hash, cntr_t& cache_hit, cntr_t& cache_miss);
    Tick read(Addr addr, cntr_t& mem_writes, cntr_t& mem_reads, cntr_t& ex_writes, cntr_t& ex_reads,
        cntr_t& hash, cntr_t& cache_hit, cntr_t& cache_miss);
    Tick write(Addr addr, cntr_t& mem_writes, cntr_t& mem_reads, cntr_t& ex_writes, cntr_t& ex_reads,
        cntr_t& hash, cntr_t& cache_hit, cntr_t& cache_miss);

private:
    //  level|cache_line_id -> sit_node*
    //lru_cache<uint64_t, sit_node*> meta_cache;
    //sit sit_tree;
    cntr_t& mem_writes;
    cntr_t& mem_reads;
    cntr_t& ex_writes;
    cntr_t& ex_reads;
    cntr_t& hash;
    cntr_t& cache_hit;
    cntr_t& cache_miss;
};

#endif
