# BMTree

## Baseline_CC
### Baseline(w/o CC)
写回时后台更新BMTree

### Strict Persist
每次写入都要严格持久化BMTree

### Atomic Update
每次写入都要更新BMTree（关键路径），并做地址跟踪

## ProMT-NP
根据论文描述模拟，DS-Cache大小为16KB
（性能存疑）

**Related readings:**
> [ProMT-NP] https://dl.acm.org/doi/10.1145/3447818.3460377?sid=SCITRUS