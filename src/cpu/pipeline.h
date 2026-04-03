#pragma once
#include "isa/decoder.h"
#include <cstdint>
#include <string>
#include <vector>

namespace cpu {

struct IFIDReg {
  isa::Decoded inst;
  uint32_t pc = 0;
  bool valid = false;
};

struct IDEXReg {
  isa::Decoded inst;
  uint32_t pc = 0;
  bool valid = false;
};

class Pipeline {
public:
  explicit Pipeline(const std::vector<uint32_t>& program = {});
  void reset();
  // step returns true if a stall/bubble was inserted this cycle
  bool step();
  uint32_t pc() const { return pc_; }
  const IFIDReg& ifid() const { return ifid_; }
  const IDEXReg& idex() const { return idex_; }
  std::string dump_regs() const;

private:
  std::vector<uint32_t> program_;
  uint32_t pc_ = 0;
  IFIDReg ifid_;
  IDEXReg idex_;
  bool last_stall_ = false;
};

} // namespace cpu
