#include "periph/tohost_mmio.h"
#include <iostream>
#include <cstdlib>

namespace periph {

ToHostMMIO::ToHostMMIO(bool exit_on_write) : val_(0), exit_on_write_(exit_on_write) {}

uint32_t ToHostMMIO::load32(uint32_t offset) {
  if (offset == 0) return static_cast<uint32_t>(val_ & 0xffffffffu);
  if (offset == 4) return static_cast<uint32_t>((val_ >> 32) & 0xffffffffu);
  return 0u;
}

void ToHostMMIO::store32(uint32_t offset, uint32_t value) {
  if (offset == 0) {
    val_ = (val_ & 0xffffffff00000000ULL) | static_cast<uint64_t>(value);
  } else if (offset == 4) {
    val_ = (val_ & 0xffffffffULL) | (static_cast<uint64_t>(value) << 32);
  }
  // if non-zero, print and optionally exit to signal host
  if (val_ != 0) {
    std::cout << "TOHOST: 0x" << std::hex << val_ << std::dec << std::endl;
    if (exit_on_write_) {
      // convention: treat value==1 as success (exit 0), otherwise non-zero -> exit code 1
      if (val_ == 1) std::exit(0);
      else std::exit(1);
    }
  }
}

} // namespace periph
