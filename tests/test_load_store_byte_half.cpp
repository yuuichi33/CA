#include <iostream>
#include <cstdint>
#include "mem/memory.h"
#include "cache/simple_cache.h"

using namespace std;

struct TestDev : public mem::Device {
  uint32_t init_val;
  uint32_t last_off = 0xffffffffu;
  uint32_t last_val = 0xffffffffu;
  TestDev(uint32_t v): init_val(v) {}
  uint32_t load32(uint32_t offset) override {
    // only offset 0 supported in this simple test
    if (offset == 0) return init_val;
    return 0;
  }
  void store32(uint32_t offset, uint32_t value) override {
    last_off = offset;
    last_val = value;
  }
};

int main() {
  // Memory basic byte/half/word ops
  mem::Memory m(16);
  m.store8(0, 0x12);
  if (m.load8(0) != 0x12) { cerr << "mem load8/store8 mismatch\n"; return 1; }

  m.store16(2, 0x3456);
  if (m.load16(2) != 0x3456) { cerr << "mem load16/store16 mismatch\n"; return 2; }

  m.store32(4, 0x11223344u);
  if (m.load32(4) != 0x11223344u) { cerr << "mem load32/store32 mismatch\n"; return 3; }

  // SimpleCache operations
  mem::Memory m2(64);
  m2.store32(0, 0x11223344u);
  SimpleCache c(&m2, 64, 16, 0);
  auto r8 = c.load8(0);
  if (r8.first != 0x44u) { cerr << "cache load8 unexpected: " << hex << r8.first << "\n"; return 4; }

  auto r16 = c.load16(1);
  if (r16.first != 0x2233u) { cerr << "cache load16 unexpected: " << hex << r16.first << "\n"; return 5; }

  c.store8(0, 0xAA);
  if (m2.load32(0) != 0x112233AAu) { cerr << "cache store8 did not update mem: " << hex << m2.load32(0) << "\n"; return 6; }

  // MMIO byte write should call device->store32 with aligned offset
  mem::Memory m3(32);
  auto *dev = new TestDev(0xAABBCCDDu);
  m3.map_device(0x1000, 0x4, dev);
  // write a byte at offset 1 within device region
  m3.store8(0x1000 + 1, 0xEE);
  if (dev->last_off != 0) { cerr << "MMIO store aligned offset mismatch: " << dev->last_off << "\n"; return 7; }
  // compute expected value manually (little-endian byte order): bytes [0]=0xDD, [1]=0xEE, [2]=0xBB, [3]=0xAA
  uint32_t expected = (0xAAu << 24) | (0xBBu << 16) | (0xEEu << 8) | 0xDDu;
  if (dev->last_val != expected) { cerr << "MMIO store32 value mismatch: got " << hex << dev->last_val << " expected " << expected << "\n"; return 8; }

  cout << "load/store byte/half tests ok\n";
  return 0;
}
