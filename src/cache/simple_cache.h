#pragma once
#include "mem/memory.h"
#include <cstdint>
#include <vector>
#include <memory>

class SimpleCache {
public:
  // mem: pointer to backing memory; cache_size in bytes; line_size in bytes; miss_latency in cycles
  SimpleCache(mem::Memory* mem, size_t cache_size = 16 * 1024, size_t line_size = 64, int miss_latency = 10);
  // returns pair<value, latency_cycles>
  std::pair<uint32_t, int> load32(uint32_t addr);
  // store32 returns latency (0 for write-through immediate)
  int store32(uint32_t addr, uint32_t value);

private:
  struct Line { std::vector<uint8_t> data; uint32_t tag = 0; bool valid = false; };
  mem::Memory* mem_;
  size_t cache_size_;
  size_t line_size_;
  size_t num_lines_;
  int miss_latency_;
  std::vector<Line> lines_;
};
