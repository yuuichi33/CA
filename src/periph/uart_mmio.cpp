#include "periph/uart_mmio.h"
#include <iostream>

namespace periph {

UARTMMIO::UARTMMIO() {}

uint32_t UARTMMIO::load32(uint32_t offset) const {
  (void)offset;
  return 0u;
}

void UARTMMIO::store32(uint32_t offset, uint32_t value) {
  (void)offset;
  uint8_t b = static_cast<uint8_t>(value & 0xffu);
  tx_.push_back(b);
  // echo to stdout for visibility
  std::cout << static_cast<char>(b);
  std::cout.flush();
}

const std::vector<uint8_t>& UARTMMIO::tx_buffer() const { return tx_; }

} // namespace periph
