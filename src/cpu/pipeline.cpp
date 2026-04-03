#include "cpu/pipeline.h"
#include <sstream>

namespace cpu {

Pipeline::Pipeline(const std::vector<uint32_t>& program) : program_(program) {
  reset();
}

void Pipeline::reset() {
  pc_ = 0;
  ifid_ = IFIDReg();
  idex_ = IDEXReg();
  last_stall_ = false;
}

bool Pipeline::step() {
  last_stall_ = false;

  bool hazard = false;
  if (idex_.valid && idex_.inst.name == "LW" && ifid_.valid) {
    uint32_t rs1 = ifid_.inst.rs1;
    uint32_t rs2 = ifid_.inst.rs2;
    if ((rs1 != 0 && rs1 == idex_.inst.rd) || (rs2 != 0 && rs2 == idex_.inst.rd)) {
      hazard = true;
    }
  }

  if (hazard) {
    isa::Decoded nop;
    nop.name = "NOP";
    nop.type = isa::InstType::OTHER;
    idex_.inst = nop;
    idex_.valid = true;
    last_stall_ = true;
    return true;
  } else {
    // advance pipeline: move IF/ID -> ID/EX
    idex_.inst = ifid_.inst;
    idex_.pc = ifid_.pc;
    idex_.valid = ifid_.valid;

    // fetch next instruction into IF/ID
    if (pc_ / 4 < program_.size()) {
      uint32_t word = program_[pc_ / 4];
      ifid_.inst = isa::decode(word);
      ifid_.pc = pc_;
      ifid_.valid = true;
      pc_ += 4;
    } else {
      ifid_.valid = false;
    }

    return false;
  }
}

std::string Pipeline::dump_regs() const {
  std::ostringstream oss;
  oss << "PC=" << pc_ << " IF/ID.valid=" << ifid_.valid << " IF/ID.inst=" << ifid_.inst.name
      << " ID/EX.valid=" << idex_.valid << " ID/EX.inst=" << idex_.inst.name;
  return oss.str();
}

} // namespace cpu
