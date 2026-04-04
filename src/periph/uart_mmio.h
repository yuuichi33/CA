#pragma once
#include "mem/memory.h"
#include <vector>
#include <deque>
#include <cstdint>

namespace cpu { class CSR; }

namespace periph {

class UARTMMIO : public mem::Device {
public:
  explicit UARTMMIO(cpu::CSR* csr = nullptr);
  uint32_t load32(uint32_t offset) override;
  void store32(uint32_t offset, uint32_t value) override;
  const std::vector<uint8_t>& tx_buffer() const;
  // inject a received byte into RX FIFO (from host/test harness)
  void inject_rx(uint8_t b);

private:
  std::vector<uint8_t> tx_;
  std::deque<uint8_t> rx_;
  cpu::CSR* csr_ = nullptr;
  uint32_t ier_ = 0; // IER: bit0 = TX IRQ enable, bit1 = RX IRQ enable
};

} // namespace periph
