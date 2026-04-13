#include <cstdint>
#include <iostream>

#include "cache/cache_config.h"
#include "cache/simple_cache.h"
#include "mem/memory.h"

static int expect_eq(const char* name, uint64_t got, uint64_t want, int rc) {
  if (got != want) {
    std::cerr << name << " mismatch: got=" << got << " want=" << want << "\n";
    return rc;
  }
  return 0;
}

static int run_conflict_miss_case() {
  mem::Memory mem(512);
  for (uint32_t a = 0; a < 128; a += 4) {
    mem.store32(a, 0x10000000u + a);
  }

  CacheConfig cfg;
  cfg.cache_size = 64;   // 4 lines (line_size=16)
  cfg.line_size = 16;
  cfg.associativity = 1; // direct-mapped
  cfg.miss_latency = 0;

  SimpleCache cache(&mem, cfg);

  // Blocks: 0,1,2,4,0 -> final access should be conflict miss.
  cache.load32(0 * 16);
  cache.load32(1 * 16);
  cache.load32(2 * 16);
  cache.load32(4 * 16);
  cache.load32(0 * 16);

  int rc = 0;
  if ((rc = expect_eq("conflict_case.misses", cache.misses(), 5, 11)) != 0) return rc;
  if ((rc = expect_eq("conflict_case.cold", cache.cold_misses(), 4, 12)) != 0) return rc;
  if ((rc = expect_eq("conflict_case.conflict", cache.conflict_misses(), 1, 13)) != 0) return rc;
  if ((rc = expect_eq("conflict_case.capacity", cache.capacity_misses(), 0, 14)) != 0) return rc;

  uint64_t classified = cache.cold_misses() + cache.conflict_misses() + cache.capacity_misses();
  if ((rc = expect_eq("conflict_case.classified_sum", classified, cache.misses(), 15)) != 0) return rc;
  return 0;
}

static int run_capacity_miss_case() {
  mem::Memory mem(512);
  for (uint32_t a = 0; a < 128; a += 4) {
    mem.store32(a, 0x20000000u + a);
  }

  CacheConfig cfg;
  cfg.cache_size = 32;   // 2 lines (line_size=16)
  cfg.line_size = 16;
  cfg.associativity = 1; // direct-mapped
  cfg.miss_latency = 0;

  SimpleCache cache(&mem, cfg);

  // Blocks: 0,1,2,0 -> final access should be capacity miss.
  cache.load32(0 * 16);
  cache.load32(1 * 16);
  cache.load32(2 * 16);
  cache.load32(0 * 16);

  int rc = 0;
  if ((rc = expect_eq("capacity_case.misses", cache.misses(), 4, 21)) != 0) return rc;
  if ((rc = expect_eq("capacity_case.cold", cache.cold_misses(), 3, 22)) != 0) return rc;
  if ((rc = expect_eq("capacity_case.conflict", cache.conflict_misses(), 0, 23)) != 0) return rc;
  if ((rc = expect_eq("capacity_case.capacity", cache.capacity_misses(), 1, 24)) != 0) return rc;

  uint64_t classified = cache.cold_misses() + cache.conflict_misses() + cache.capacity_misses();
  if ((rc = expect_eq("capacity_case.classified_sum", classified, cache.misses(), 25)) != 0) return rc;
  return 0;
}

static int run_store_path_case() {
  mem::Memory mem(512);
  CacheConfig cfg;
  cfg.cache_size = 32;
  cfg.line_size = 16;
  cfg.associativity = 1;
  cfg.miss_latency = 0;

  SimpleCache cache(&mem, cfg);

  // Use stores to ensure miss classification is also tracked on write path.
  cache.store8(0 * 16, 0x11);
  cache.store8(1 * 16, 0x22);
  cache.store8(2 * 16, 0x33);
  cache.store8(0 * 16, 0x44);

  int rc = 0;
  if ((rc = expect_eq("store_case.misses", cache.misses(), 4, 31)) != 0) return rc;
  if ((rc = expect_eq("store_case.cold", cache.cold_misses(), 3, 32)) != 0) return rc;
  if ((rc = expect_eq("store_case.conflict", cache.conflict_misses(), 0, 33)) != 0) return rc;
  if ((rc = expect_eq("store_case.capacity", cache.capacity_misses(), 1, 34)) != 0) return rc;

  uint64_t classified = cache.cold_misses() + cache.conflict_misses() + cache.capacity_misses();
  if ((rc = expect_eq("store_case.classified_sum", classified, cache.misses(), 35)) != 0) return rc;
  return 0;
}

int main() {
  int rc = run_conflict_miss_case();
  if (rc != 0) return rc;

  rc = run_capacity_miss_case();
  if (rc != 0) return rc;

  rc = run_store_path_case();
  if (rc != 0) return rc;

  std::cout << "cache miss type tests ok\n";
  return 0;
}
