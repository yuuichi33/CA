#pragma once
#include "mem/memory.h"
#include "periph/timer.h"
#include <cstdint>

namespace periph {

class TimerMMIO : public mem::Device {
public:
  explicit TimerMMIO(Timer* t);
  uint32_t load32(uint32_t offset) override;
  void store32(uint32_t offset, uint32_t value) override;

private:
  Timer* timer_;
  uint64_t mtimecmp_;
};

} // namespace periph
