#include <iostream>
#include "isa/decoder.h"
#include "cpu/single_cycle.h"

using namespace isa;
using namespace cpu;

static uint32_t encodeR(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeI(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfff;
  return (uimm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeB(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0x1fff;
  uint32_t imm12 = (uimm >> 12) & 0x1;
  uint32_t imm11 = (uimm >> 11) & 0x1;
  uint32_t imm10_5 = (uimm >> 5) & 0x3f;
  uint32_t imm4_1 = (uimm >> 1) & 0xf;
  return (imm12 << 31) | (imm11 << 7) | (imm10_5 << 25) | (imm4_1 << 8) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | opcode;
}

int main() {
  SingleCycle cpu;

  // Program:
  // ADDI x1, x0, 5
  // ADDI x2, x0, 3
  // ADD  x3, x1, x2
  // SUB  x4, x1, x2
  // BEQ  x1, x2, 8   (not taken)
  // ADDI x5, x0, 9
  // ADDI x6, x0, 9
  // BEQ  x5, x6, 8   (taken)

  uint32_t prog[] = {
    encodeI(5, 0, 0x0, 1, 0x13),
    encodeI(3, 0, 0x0, 2, 0x13),
    encodeR(0x00, 2, 1, 0x0, 3, 0x33),
    encodeR(0x20, 2, 1, 0x0, 4, 0x33),
    encodeB(8, 2, 1, 0x0, 0x63),
    encodeI(9, 0, 0x0, 5, 0x13),
    encodeI(9, 0, 0x0, 6, 0x13),
    encodeB(8, 6, 5, 0x0, 0x63)
  };

  const size_t N = sizeof(prog)/sizeof(prog[0]);
  for (size_t i = 0; i < N; ++i) {
    auto d = isa::decode(prog[i]);
    cpu.execute(d);
  }

  bool ok = true;
  if (cpu.regs().read(1) != 5) { std::cerr << "x1 != 5\n"; ok = false; }
  if (cpu.regs().read(2) != 3) { std::cerr << "x2 != 3\n"; ok = false; }
  if (cpu.regs().read(3) != 8) { std::cerr << "x3 != 8\n"; ok = false; }
  if (cpu.regs().read(4) != 2) { std::cerr << "x4 != 2\n"; ok = false; }

  // After the second BEQ (taken) PC should be 36
  if (cpu.pc() != 36) { std::cerr << "PC != 36, got " << cpu.pc() << "\n"; ok = false; }

  return ok ? 0 : 1;
}
