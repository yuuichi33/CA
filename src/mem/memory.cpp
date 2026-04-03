#include "mem/memory.h"
#include <stdexcept>
#include <cstring>

namespace mem {

Memory::Memory(size_t sz) : data_(sz, 0) {}

uint32_t Memory::load32(uint32_t addr) const {
  if (addr + 4 > data_.size()) throw std::out_of_range("memory read out of bounds");
  uint32_t val;
  std::memcpy(&val, &data_[addr], 4);
  return val;
}

void Memory::store32(uint32_t addr, uint32_t value) {
  if (addr + 4 > data_.size()) throw std::out_of_range("memory write out of bounds");
  std::memcpy(&data_[addr], &value, 4);
}

} // namespace mem
