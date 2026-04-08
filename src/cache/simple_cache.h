#pragma once
#include "mem/memory.h"
#include <cstdint>
#include <vector>
#include <memory>
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
};
