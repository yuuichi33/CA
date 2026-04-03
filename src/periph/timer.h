#pragma once
#include <cstdint>

namespace periph {

class Timer {
public:
  Timer();
  void set_interval(uint32_t interval);
  void set_enabled(bool en);
  bool tick();
  void reset();

private:
  uint64_t ticks_;
  uint32_t interval_;
  bool enabled_;
};

} // namespace periph
