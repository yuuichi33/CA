#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace mem {

class Memory {
public:
  explicit Memory(size_t sz = 64 * 1024);
  uint32_t load32(uint32_t addr) const;
  void store32(uint32_t addr, uint32_t value);

private:
  std::vector<uint8_t> data_;
};

} // namespace mem
