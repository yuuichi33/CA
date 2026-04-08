#pragma once
#include <cstddef>
#include <cstdint>

struct CacheConfig {
  bool enabled = true;
  size_t cache_size = 16 * 1024;
  size_t line_size = 64;
  unsigned associativity = 1; // 1 = direct-mapped
  bool write_back = false;    // write-through by default
  bool write_allocate = false;
  int miss_latency = 10;
};
