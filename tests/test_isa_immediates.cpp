#include <iostream>
#include <vector>
#include "cpu/pipeline.h"

using namespace cpu;

static uint32_t encodeI(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfff;
  return (uimm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

// encode I-type shift immediate with funct7
static uint32_t encodeI_shamt(uint32_t shamt, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode, uint32_t funct7) {
  return ((funct7 & 0x7f) << 25) | ((shamt & 0x1f) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

int main() {
  // program: test ADDI, XORI, ORI, ANDI, SLTI, SLTIU, SRLI, SRAI, SLLI
  std::vector<uint32_t> prog;
  // ADDI x1,x0,5
  prog.push_back(encodeI(5, 0, 0x0, 1, 0x13));
  // XORI x2,x1,3 -> 5 ^ 3 = 6
  prog.push_back(encodeI(3, 1, 0x4, 2, 0x13));
  // ORI x3,x1,2 -> 5 | 2 = 7
  prog.push_back(encodeI(2, 1, 0x6, 3, 0x13));
  // ANDI x4,x1,1 -> 5 & 1 = 1
  prog.push_back(encodeI(1, 1, 0x7, 4, 0x13));
  // SLTI x5,x1,6 -> (5 < 6) = 1
  prog.push_back(encodeI(6, 1, 0x2, 5, 0x13));
  // SLTIU x6,x1,6 -> (5 < 6) = 1
  prog.push_back(encodeI(6, 1, 0x3, 6, 0x13));
  // SLLI x7,x1,1 -> 5 << 1 = 10 (funct7 == 0)
  prog.push_back(encodeI_shamt(1, 1, 0x1, 7, 0x13, 0x00));
  // SRLI x8,x1,1 -> 5 >> 1 = 2 (logical)
  prog.push_back(encodeI_shamt(1, 1, 0x5, 8, 0x13, 0x00));
  // SRAI x9,x1,1 -> 5 >> 1 = 2 (arithmetic)
  prog.push_back(encodeI_shamt(1, 1, 0x5, 9, 0x13, 0x20));

  Pipeline p(prog);
  for (int i = 0; i < 50; ++i) p.step();

  if (p.regs().read(1) != 5) { std::cerr << "ADDI failed\n"; return 1; }
  if (p.regs().read(2) != 6) { std::cerr << "XORI failed got " << p.regs().read(2) << "\n"; return 2; }
  if (p.regs().read(3) != 7) { std::cerr << "ORI failed got " << p.regs().read(3) << "\n"; return 3; }
  if (p.regs().read(4) != 1) { std::cerr << "ANDI failed got " << p.regs().read(4) << "\n"; return 4; }
  if (p.regs().read(5) != 1) { std::cerr << "SLTI failed got " << p.regs().read(5) << "\n"; return 5; }
  if (p.regs().read(6) != 1) { std::cerr << "SLTIU failed got " << p.regs().read(6) << "\n"; return 6; }
  if (p.regs().read(7) != 10) { std::cerr << "SLLI failed got " << p.regs().read(7) << "\n"; return 7; }
  if (p.regs().read(8) != 2) { std::cerr << "SRLI failed got " << p.regs().read(8) << "\n"; return 8; }
  if (p.regs().read(9) != 2) { std::cerr << "SRAI failed got " << p.regs().read(9) << "\n"; return 9; }

  std::cout << "isa immediates tests ok\n";
  return 0;
}
