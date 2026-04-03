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
  uint64_t ticks() const;
  void set_mtimecmp(uint64_t v);
  uint64_t mtimecmp() const;
  uint32_t interval() const;
  bool enabled() const;

private:
  uint64_t ticks_;
  uint32_t interval_;
  bool enabled_;
  uint64_t mtimecmp_;
};

} // namespace periph
