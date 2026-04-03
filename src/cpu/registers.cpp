#include "cpu/registers.h"
#include <sstream>

namespace cpu {

RegisterFile::RegisterFile() { regs_.fill(0u); regs_[0] = 0u; }

uint32_t RegisterFile::read(unsigned idx) const {
  if (idx >= regs_.size()) return 0u;
  if (idx == 0) return 0u;
  return regs_[idx];
}

void RegisterFile::write(unsigned idx, uint32_t val) {
  if (idx == 0) return; // x0 is hardwired zero
  if (idx >= regs_.size()) return;
  regs_[idx] = val;
}

std::string RegisterFile::dump() const {
  std::ostringstream oss;
  for (size_t i = 0; i < regs_.size(); ++i) {
    oss << "x" << i << ":" << regs_[i];
    if (i + 1 < regs_.size()) oss << ", ";
  }
  return oss.str();
}

} // namespace cpu
