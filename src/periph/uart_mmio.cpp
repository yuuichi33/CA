#include "periph/uart_mmio.h"
#include "cpu/csr.h"
#include <iostream>

namespace periph {

UARTMMIO::UARTMMIO(cpu::CSR* csr) : csr_(csr) {}

uint32_t UARTMMIO::load32(uint32_t offset) {
  switch (offset) {
    case 0x00: { // DATA read (RX)
      if (rx_.empty()) return 0u;
      uint8_t b = rx_.front();
      rx_.pop_front();
      // if RX queue now empty, clear pending mip
      if (rx_.empty() && csr_) csr_->set_mip_uart(false);
      return static_cast<uint32_t>(b);
    }
    case 0x04: // IER
      return ier_;
    case 0x08: // STATUS
      // bit0 = TX ready (always ready), bit1 = RX ready
      return (1u) | (rx_.empty() ? 0u : 2u);
    default:
      return 0u;
  }
}

void UARTMMIO::store32(uint32_t offset, uint32_t value) {
  switch (offset) {
    case 0x00: {
      uint8_t b = static_cast<uint8_t>(value & 0xffu);
      tx_.push_back(b);
      std::cout << static_cast<char>(b);
      std::cout.flush();
      // if TX IRQ enabled, set UART MIP
      if (csr_ && (ier_ & 0x1)) {
        csr_->set_mip_uart(true);
      }
      break;
    }
    case 0x04: // IER
      ier_ = value;
      break;
    default:
      break;
  }
}

const std::vector<uint8_t>& UARTMMIO::tx_buffer() const { return tx_; }

void UARTMMIO::inject_rx(uint8_t b) {
  rx_.push_back(b);
  // if RX interrupt enabled, set CSR mip_uart
  if (csr_ && (ier_ & 0x2)) {
    csr_->set_mip_uart(true);
  }
}

} // namespace periph
