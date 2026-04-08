#include <iostream>
#include <cstdint>
#include "mem/memory.h"
#include "cache/simple_cache.h"
#include "cache/cache_config.h"

int main() {
  mem::Memory m(256);
  // initial memory contents
  m.store32(0, 0x11223344u);
  m.store32(32, 0x77777777u);

  CacheConfig cfg;
  cfg.cache_size = 32; // two lines of 16 bytes
  cfg.line_size = 16;
  cfg.associativity = 1; // direct-mapped for predictable eviction
  cfg.write_back = true;
  cfg.write_allocate = true;
  cfg.miss_latency = 0;

  SimpleCache c(&m, cfg);

  // load block 0 into cache, then store (should mark dirty, not write-through)
  c.load32(0);
  c.store32(0, 0xAABBCCDDu);

  // access block 2 (addr = 2 * line_size = 32) which maps to same set -> causes eviction
  c.load32(32);

  // after eviction, dirty line should have been written back
  if (m.load32(0) != 0xAABBCCDDu) {
    std::cerr << "write-back eviction failed: mem[0]=" << std::hex << m.load32(0) << "\n";
    return 1;
  }

  std::cout << "cache write-back test ok\n";
  return 0;
}
