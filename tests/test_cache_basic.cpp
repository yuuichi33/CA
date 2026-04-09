#include <iostream>
#include <cstdint>
#include "mem/memory.h"
#include "cache/simple_cache.h"
#include "cache/cache_config.h"

int main() {
  mem::Memory m(64);
  m.store32(0, 0x11223344u);

  CacheConfig cfg;
  cfg.cache_size = 64;
  cfg.line_size = 16;
  cfg.miss_latency = 0;

  SimpleCache c(&m, cfg);

  auto r8 = c.load8(0);
  if (r8.first != 0x44u) { std::cerr << "cache load8 unexpected: " << std::hex << r8.first << "\n"; return 1; }
  auto r8_hit = c.load8(0);
  if (r8_hit.first != 0x44u) { std::cerr << "cache second load8 unexpected: " << std::hex << r8_hit.first << "\n"; return 1; }

  if (c.misses() == 0 || c.hits() == 0) {
    std::cerr << "cache stats not updated as expected: hits=" << c.hits() << " misses=" << c.misses() << "\n";
    return 2;
  }

  // store32 should update backing memory (write-through default)
  c.store32(0, 0xAABBCCDDu);
  if (m.load32(0) != 0xAABBCCDDu) { std::cerr << "cache store32 did not update mem: " << std::hex << m.load32(0) << "\n"; return 3; }

  std::cout << "cache basic tests ok\n";
  return 0;
}
