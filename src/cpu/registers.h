#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace cpu {

class RegisterFile {
public:
  RegisterFile();
  uint32_t read(unsigned idx) const;
  void write(unsigned idx, uint32_t val);
  std::string dump() const;

private:
  std::array<uint32_t, 32> regs_{};
};

} // namespace cpu
