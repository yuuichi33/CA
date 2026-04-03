#include "cpu/pipeline.h"
#include <sstream>

namespace cpu {

Pipeline::Pipeline(const std::vector<uint32_t>& program, size_t mem_size) : program_(program), mem_(mem_size) {
  reset();
  // instantiate caches
  // default miss latency set to 0 for tests; can be tuned later
  icache_ = new SimpleCache(&mem_, 16 * 1024, 64, 0);
  dcache_ = new SimpleCache(&mem_, 16 * 1024, 64, 0);
  // copy program into physical memory at address 0
  for (size_t i = 0; i < program_.size(); ++i) {
    mem_.store32(static_cast<uint32_t>(i * 4), program_[i]);
  }
}

void Pipeline::reset() {
  pc_ = 0;
  ifid_ = IFIDReg();
  idex_ = IDEXReg();
  exmem_ = EXMEMReg();
  memwb_ = MEMWBReg();
  regs_ = RegisterFile();
  last_stall_ = false;
}

bool Pipeline::step() {
  last_stall_ = false;

  // if currently stalling for cache miss, countdown
  if (stall_counter_ > 0) {
    --stall_counter_;
    if (stall_counter_ == 0) {
      if (stall_kind_ == StallKind::If) {
        // commit pending IF fetch
        ifid_.inst = isa::decode(pending_if_word_);
        ifid_.pc = pending_if_pc_;
        ifid_.valid = true;
        stall_kind_ = StallKind::None;
      } else if (stall_kind_ == StallKind::MemLoad) {
        // commit pending memory load into memwb_
        pending_memwb_.mem_data = pending_mem_value_;
        memwb_ = pending_memwb_;
        stall_kind_ = StallKind::None;
      }
    }
    return true;
  }

  // Snapshot previous stage registers
  IFIDReg prev_ifid = ifid_;
  IDEXReg prev_idex = idex_;
  EXMEMReg prev_exmem = exmem_;
  MEMWBReg prev_memwb = memwb_;

  // ------------------ WB stage ------------------
  if (prev_memwb.valid && prev_memwb.write_back && prev_memwb.rd != 0) {
    if (prev_memwb.inst.name == "LW") {
      regs_.write(prev_memwb.rd, prev_memwb.mem_data);
    } else {
      regs_.write(prev_memwb.rd, prev_memwb.alu_result);
    }
  }

  // ------------------ MEM stage ------------------
  memwb_ = MEMWBReg();
  if (prev_exmem.valid) {
    memwb_.inst = prev_exmem.inst;
    memwb_.pc = prev_exmem.pc;
    memwb_.valid = true;
    memwb_.rd = prev_exmem.rd;
    memwb_.write_back = prev_exmem.write_back;
    memwb_.alu_result = prev_exmem.alu_result;

    if (prev_exmem.inst.name == "LW") {
      // use dcache for data access
      auto res = dcache_->load32(prev_exmem.alu_result);
      if (res.second == 0) {
        memwb_.mem_data = res.first;
      } else {
        // miss: stall pipeline for res.second cycles then deliver
        stall_counter_ = res.second;
        stall_kind_ = StallKind::MemLoad;
        pending_memwb_ = memwb_;
        pending_mem_value_ = res.first;
        return true;
      }
    } else if (prev_exmem.inst.name == "SW") {
      dcache_->store32(prev_exmem.alu_result, prev_exmem.rs2_val);
    }
  }

  // ------------------ EX stage ------------------
  exmem_ = EXMEMReg();
  bool branch_taken = false;
  uint32_t branch_target = 0;
  if (prev_idex.valid) {
    // operand fetch with forwarding: prioritize EX/MEM, then MEM/WB
    uint32_t a = prev_idex.rs1_val;
    uint32_t b = prev_idex.rs2_val;

    // forward for rs1
    if (prev_exmem.valid && prev_exmem.rd != 0 && prev_exmem.write_back && prev_exmem.rd == prev_idex.inst.rs1 && prev_exmem.inst.name != "LW") {
      a = prev_exmem.alu_result;
    } else if (prev_memwb.valid && prev_memwb.rd != 0 && prev_memwb.write_back && prev_memwb.rd == prev_idex.inst.rs1) {
      if (prev_memwb.inst.name == "LW") a = prev_memwb.mem_data; else a = prev_memwb.alu_result;
    }

    // forward for rs2
    if (prev_exmem.valid && prev_exmem.rd != 0 && prev_exmem.write_back && prev_exmem.rd == prev_idex.inst.rs2 && prev_exmem.inst.name != "LW") {
      b = prev_exmem.alu_result;
    } else if (prev_memwb.valid && prev_memwb.rd != 0 && prev_memwb.write_back && prev_memwb.rd == prev_idex.inst.rs2) {
      if (prev_memwb.inst.name == "LW") b = prev_memwb.mem_data; else b = prev_memwb.alu_result;
    }

    uint32_t alu_res = 0;
    const std::string &name = prev_idex.inst.name;
    if (name == "ADD") alu_res = a + b;
    else if (name == "SUB") alu_res = a - b;
    else if (name == "ADDI") alu_res = a + static_cast<uint32_t>(prev_idex.inst.imm);
    else if (name == "SLLI") alu_res = a << (static_cast<uint32_t>(prev_idex.inst.imm) & 0x1f);
    else if (name == "SLL") alu_res = a << (b & 0x1f);
    else if (name == "SRL") alu_res = a >> (b & 0x1f);
    else if (name == "SRA") alu_res = static_cast<uint32_t>(static_cast<int32_t>(a) >> (b & 0x1f));
    else if (name == "OR") alu_res = a | b;
    else if (name == "AND") alu_res = a & b;
    else if (name == "XOR") alu_res = a ^ b;
    else if (name == "SLT") alu_res = (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? 1u : 0u;
    else if (name == "SLTU") alu_res = (a < b) ? 1u : 0u;
    else if (name == "LW" || name == "SW") alu_res = a + static_cast<uint32_t>(prev_idex.inst.imm);
    else if (name == "BEQ") {
      if (a == b) { branch_taken = true; branch_target = prev_idex.pc + prev_idex.inst.imm; }
    } else if (name == "BNE") {
      if (a != b) { branch_taken = true; branch_target = prev_idex.pc + prev_idex.inst.imm; }
    } else if (name == "JAL") {
      alu_res = prev_idex.pc + 4;
      branch_taken = true;
      branch_target = prev_idex.pc + prev_idex.inst.imm;
    } else if (name == "JALR") {
      alu_res = prev_idex.pc + 4;
      branch_taken = true;
      branch_target = (a + static_cast<uint32_t>(prev_idex.inst.imm)) & ~1u;
    }

    exmem_.inst = prev_idex.inst;
    exmem_.pc = prev_idex.pc;
    exmem_.valid = prev_idex.valid;
    exmem_.alu_result = alu_res;
    exmem_.rs2_val = prev_idex.rs2_val;
    exmem_.rd = prev_idex.inst.rd;
    // determine if instruction writes back
    exmem_.write_back = (name == "ADD" || name == "SUB" || name == "ADDI" || name == "SLLI" || name == "SLL" || name == "SRL" || name == "SRA" || name == "OR" || name == "AND" || name == "XOR" || name == "SLT" || name == "SLTU" || name == "LW" || name == "JAL" || name == "JALR" || name == "LUI" || name == "AUIPC");

    if (branch_taken) {
      pc_ = branch_target;
      // flush IF/ID (will be handled by not fetching below)
      ifid_.valid = false;
    }
  }

  // ------------------ ID and IF stage ------------------
  // hazard detection: load-use (prev_idex is a load and prev_ifid uses its rd)
  bool hazard = false;
  if (prev_idex.valid && prev_idex.inst.name == "LW" && prev_ifid.valid) {
    uint32_t rs1 = prev_ifid.inst.rs1;
    uint32_t rs2 = prev_ifid.inst.rs2;
    if ((rs1 != 0 && rs1 == prev_idex.inst.rd) || (rs2 != 0 && rs2 == prev_idex.inst.rd)) {
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
    // do not fetch, keep ifid_ unchanged
  } else {
    // move IF/ID -> ID/EX and read regs (note: WB already happened earlier this cycle)
    if (prev_ifid.valid) {
      idex_.inst = prev_ifid.inst;
      idex_.pc = prev_ifid.pc;
      idex_.valid = prev_ifid.valid;
      idex_.rs1_val = regs_.read(prev_ifid.inst.rs1);
      idex_.rs2_val = regs_.read(prev_ifid.inst.rs2);
    } else {
      idex_ = IDEXReg();
    }

    // fetch next instruction into IF/ID unless we flushed due to branch
    if (pc_ / 4 < program_.size()) {
      // use icache for instruction fetch
      auto res = icache_->load32(pc_);
      if (res.second == 0) {
        uint32_t word = res.first;
        ifid_.inst = isa::decode(word);
        ifid_.pc = pc_;
        ifid_.valid = true;
        pc_ += 4;
      } else {
        // miss: stall pipeline for res.second cycles then deliver
        stall_counter_ = res.second;
        stall_kind_ = StallKind::If;
        pending_if_word_ = res.first;
        pending_if_pc_ = pc_;
        return true;
      }
    } else {
      ifid_.valid = false;
    }
  }

  return last_stall_;
}

std::string Pipeline::dump_regs() const {
  std::ostringstream oss;
  oss << "PC=" << pc_ << " IF/ID.valid=" << ifid_.valid << " IF/ID.inst=" << ifid_.inst.name
      << " ID/EX.valid=" << idex_.valid << " ID/EX.inst=" << idex_.inst.name;
  return oss.str();
}

} // namespace cpu
