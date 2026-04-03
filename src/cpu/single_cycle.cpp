#include "cpu/single_cycle.h"
#include "cpu/alu.h"

using namespace cpu;

SingleCycle::SingleCycle() { reset(); }

void SingleCycle::reset() {
  regs_ = RegisterFile();
  pc_ = 0u;
}

uint32_t SingleCycle::pc() const { return pc_; }

void SingleCycle::setPC(uint32_t address) { pc_ = address; }

RegisterFile& SingleCycle::regs() { return regs_; }

void SingleCycle::execute(const isa::Decoded& d) {
  const auto rd = d.rd;
  const auto rs1 = d.rs1;
  const auto rs2 = d.rs2;
  const auto imm = static_cast<uint32_t>(d.imm);

  const std::string &name = d.name;

  if (name == "ADD") {
    regs_.write(rd, regs_.read(rs1) + regs_.read(rs2));
    pc_ += 4;
    return;
  }

  if (name == "SUB") {
    regs_.write(rd, regs_.read(rs1) - regs_.read(rs2));
    pc_ += 4;
    return;
  }

  if (name == "ADDI") {
    regs_.write(rd, regs_.read(rs1) + imm);
    pc_ += 4;
    return;
  }

  if (name == "SLLI") {
    regs_.write(rd, regs_.read(rs1) << (imm & 0x1f));
    pc_ += 4;
    return;
  }

  if (name == "BEQ") {
    if (regs_.read(rs1) == regs_.read(rs2)) pc_ = pc_ + d.imm;
    else pc_ += 4;
    return;
  }

  if (name == "BNE") {
    if (regs_.read(rs1) != regs_.read(rs2)) pc_ = pc_ + d.imm;
    else pc_ += 4;
    return;
  }

  if (name == "JAL") {
    regs_.write(rd, pc_ + 4);
    pc_ = pc_ + d.imm;
    return;
  }

  if (name == "JALR") {
    regs_.write(rd, pc_ + 4);
    pc_ = (regs_.read(rs1) + d.imm) & ~1u;
    return;
  }

  if (name == "LUI") {
    regs_.write(rd, imm);
    pc_ += 4;
    return;
  }

  // I-type logicals
  if (name == "ORI") { regs_.write(rd, regs_.read(rs1) | imm); pc_ += 4; return; }
  if (name == "ANDI") { regs_.write(rd, regs_.read(rs1) & imm); pc_ += 4; return; }
  if (name == "XORI") { regs_.write(rd, regs_.read(rs1) ^ imm); pc_ += 4; return; }

  // R-type logical shift / logic
  if (name == "SLL") { regs_.write(rd, regs_.read(rs1) << (regs_.read(rs2) & 0x1f)); pc_ += 4; return; }
  if (name == "SRL") { regs_.write(rd, regs_.read(rs1) >> (regs_.read(rs2) & 0x1f)); pc_ += 4; return; }
  if (name == "SRA") { regs_.write(rd, static_cast<uint32_t>(static_cast<int32_t>(regs_.read(rs1)) >> (regs_.read(rs2) & 0x1f))); pc_ += 4; return; }
  if (name == "OR") { regs_.write(rd, regs_.read(rs1) | regs_.read(rs2)); pc_ += 4; return; }
  if (name == "AND") { regs_.write(rd, regs_.read(rs1) & regs_.read(rs2)); pc_ += 4; return; }
  if (name == "XOR") { regs_.write(rd, regs_.read(rs1) ^ regs_.read(rs2)); pc_ += 4; return; }
  if (name == "SLT") { regs_.write(rd, (static_cast<int32_t>(regs_.read(rs1)) < static_cast<int32_t>(regs_.read(rs2))) ? 1u : 0u); pc_ += 4; return; }
  if (name == "SLTU") { regs_.write(rd, (regs_.read(rs1) < regs_.read(rs2)) ? 1u : 0u); pc_ += 4; return; }

  // Unknown/unsupported: advance PC
  pc_ += 4;
}
