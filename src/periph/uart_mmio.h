#pragma once
#include "mem/memory.h"
#include <vector>
#include <cstdint>

namespace periph {

class UARTMMIO : public mem::Device {
public:
  UARTMMIO();
  uint32_t load32(uint32_t offset) const override;
  void store32(uint32_t offset, uint32_t value) override;
  const std::vector<uint8_t>& tx_buffer() const;

private:
  std::vector<uint8_t> tx_;
};

} // namespace periph
