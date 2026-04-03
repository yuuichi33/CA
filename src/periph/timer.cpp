#include "periph/timer.h"

namespace periph {

Timer::Timer() : ticks_(0), interval_(100), enabled_(false) {}

void Timer::set_interval(uint32_t interval) { interval_ = (interval == 0 ? 1u : interval); }
void Timer::set_enabled(bool en) { enabled_ = en; }
void Timer::reset() { ticks_ = 0; }

bool Timer::tick() {
  if (!enabled_) return false;
  ++ticks_;
  if (interval_ > 0 && (ticks_ % interval_) == 0) return true;
  return false;
}

} // namespace periph
