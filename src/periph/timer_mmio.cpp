#include "periph/timer_mmio.h"

namespace periph {

TimerMMIO::TimerMMIO(Timer* t) : timer_(t), mtimecmp_(0) {}

uint32_t TimerMMIO::load32(uint32_t offset) const {
  switch (offset) {
    case 0x00: return static_cast<uint32_t>(timer_->ticks() & 0xffffffffu);
    case 0x04: return static_cast<uint32_t>((timer_->ticks() >> 32) & 0xffffffffu);
    case 0x08: return static_cast<uint32_t>(mtimecmp_ & 0xffffffffu);
    case 0x0C: return static_cast<uint32_t>((mtimecmp_ >> 32) & 0xffffffffu);
    case 0x10: return timer_->interval();
    case 0x14: return timer_->enabled() ? 1u : 0u;
    default: return 0u;
  }
}

void TimerMMIO::store32(uint32_t offset, uint32_t value) {
  switch (offset) {
    case 0x08:
      mtimecmp_ = (mtimecmp_ & 0xffffffff00000000ULL) | static_cast<uint64_t>(value);
      timer_->set_mtimecmp(mtimecmp_);
      break;
    case 0x0C:
      mtimecmp_ = (mtimecmp_ & 0xffffffffULL) | (static_cast<uint64_t>(value) << 32);
      timer_->set_mtimecmp(mtimecmp_);
      break;
    case 0x10:
      timer_->set_interval(value);
      break;
    case 0x14:
      timer_->set_enabled(value != 0);
      break;
    default:
      break;
  }
}

} // namespace periph
