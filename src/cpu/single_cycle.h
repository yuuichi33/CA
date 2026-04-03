#pragma once
#include "cpu/registers.h"
#include "isa/decoder.h"
#include <cstdint>

namespace cpu {

class SingleCycle {
public:
  SingleCycle();
  void reset();
  uint32_t pc() const;
  void setPC(uint32_t address);
  void execute(const isa::Decoded& d);
  RegisterFile& regs();

private:
  RegisterFile regs_;
  uint32_t pc_ = 0;
};

} // namespace cpu
