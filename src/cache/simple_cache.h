#pragma once
#include "mem/memory.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_set>
#include "cache/cache_config.h"

class SimpleCache {
public:
  // mem: pointer to backing memory; cache_size in bytes; line_size in bytes; miss_latency in cycles
  SimpleCache(mem::Memory* mem, size_t cache_size = 16 * 1024, size_t line_size = 64, int miss_latency = 10);
  // Construct from a CacheConfig
  SimpleCache(mem::Memory* mem, const CacheConfig& cfg);
  // returns pair<value, latency_cycles>
  std::pair<uint32_t, int> load32(uint32_t addr);
  std::pair<uint32_t, int> load8(uint32_t addr);
  std::pair<uint32_t, int> load16(uint32_t addr);
  // store32 returns latency (0 for write-through immediate)
  int store32(uint32_t addr, uint32_t value);
  int store8(uint32_t addr, uint8_t value);
  int store16(uint32_t addr, uint16_t value);
  // cache maintenance
  void flush_all();      // write back dirty lines (if write-back) and invalidate
  void invalidate_all(); // invalidate lines without writeback

private:
  struct Line { std::vector<uint8_t> data; uint32_t tag = 0; bool valid = false; uint32_t lru = 0; bool dirty = false; };
  struct ShadowLine { bool valid = false; uint32_t block = 0; uint32_t lru = 0; };
  mem::Memory* mem_;
  size_t cache_size_;
  size_t line_size_;
  size_t associativity_ = 1;
  size_t num_sets_ = 0;
  int miss_latency_;
  // sets_[set_idx][way]
  std::vector<std::vector<Line>> sets_;
  uint32_t access_counter_ = 0;
  bool write_back_ = false;
  bool write_allocate_ = false;
  // statistics
  uint64_t accesses_ = 0;
  uint64_t hits_ = 0;
  uint64_t misses_ = 0;
  uint64_t evictions_ = 0;
  uint64_t writebacks_ = 0;
  uint64_t cold_misses_ = 0;
  uint64_t conflict_misses_ = 0;
  uint64_t capacity_misses_ = 0;
  // For miss classification: track seen blocks and simulate an equal-capacity fully-associative cache.
  std::unordered_set<uint32_t> ever_seen_blocks_;
  std::vector<ShadowLine> shadow_lines_;
  uint32_t shadow_access_counter_ = 0;

  bool shadow_access(uint32_t block);
  void account_miss_class(uint32_t block, bool shadow_hit);
public:
  uint64_t accesses() const { return accesses_; }
  uint64_t hits() const { return hits_; }
  uint64_t misses() const { return misses_; }
  uint64_t evictions() const { return evictions_; }
  uint64_t writebacks() const { return writebacks_; }
  uint64_t cold_misses() const { return cold_misses_; }
  uint64_t conflict_misses() const { return conflict_misses_; }
  uint64_t capacity_misses() const { return capacity_misses_; }
  void reset_stats() {
    accesses_ = hits_ = misses_ = evictions_ = writebacks_ = 0;
    cold_misses_ = conflict_misses_ = capacity_misses_ = 0;
  }
};
