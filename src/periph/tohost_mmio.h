#pragma once
#include "mem/memory.h"
#include <cstdint>
#include <atomic>

namespace periph {

class ToHostMMIO : public mem::Device {
public:
  explicit ToHostMMIO(bool exit_on_write = true);
  uint32_t load32(uint32_t offset) override;
  void store32(uint32_t offset, uint32_t value) override;

private:
  uint64_t val_ = 0;
  bool exit_on_write_ = true;
};

} // namespace periph
