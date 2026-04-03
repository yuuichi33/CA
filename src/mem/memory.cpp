#include "mem/memory.h"
#include <stdexcept>
#include <cstring>

#include <algorithm>

namespace mem {

Memory::Memory(size_t sz) : data_(sz, 0) {}

uint32_t Memory::load32(uint32_t addr) const {
  // check MMIO mappings first
  for (const auto &m : mappings_) {
    if (addr >= m.base && addr + 4 <= m.base + m.size) {
      return m.dev->load32(addr - m.base);
    }
  }

  if (addr + 4 > data_.size()) throw std::out_of_range("memory read out of bounds");
  uint32_t val;
  std::memcpy(&val, &data_[addr], 4);
  return val;
}

void Memory::store32(uint32_t addr, uint32_t value) {
  // check MMIO mappings first
  for (const auto &m : mappings_) {
    if (addr >= m.base && addr + 4 <= m.base + m.size) {
      m.dev->store32(addr - m.base, value);
      return;
    }
  }

  if (addr + 4 > data_.size()) throw std::out_of_range("memory write out of bounds");
  std::memcpy(&data_[addr], &value, 4);
}

void Memory::map_device(uint32_t base, uint32_t size, Device* dev) {
  // overwrite any existing mapping at same base
  mappings_.push_back(Mapping{base, size, dev});
}

Memory::~Memory() {
  for (auto &m : mappings_) delete m.dev;
}

} // namespace mem
