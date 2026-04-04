#include <iostream>
#include <vector>
#include "cpu/pipeline.h"

using namespace cpu;

static uint32_t encodeR(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeI(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfff;
  return (uimm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeS(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfff;
  uint32_t imm11_5 = (uimm >> 5) & 0x7f;
  uint32_t imm4_0 = uimm & 0x1f;
  return (imm11_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (imm4_0 << 7) | opcode;
}

int main() {
  // Test 1: SB then LB/LBU
  std::vector<uint32_t> prog1;
  prog1.push_back(encodeI(32, 0, 0x0, 1, 0x13)); // ADDI x1,x0,32
  prog1.push_back(encodeI(0xFF, 0, 0x0, 2, 0x13)); // ADDI x2,x0,255
  prog1.push_back(encodeS(1, 2, 1, 0x0, 0x23)); // SB x2,1(x1)
  prog1.push_back(encodeI(1, 1, 0x0, 3, 0x03)); // LB x3,1(x1)
  prog1.push_back(encodeI(1, 1, 0x4, 4, 0x03)); // LBU x4,1(x1)

  Pipeline p1(prog1);
  // run enough cycles for all instructions to complete
  for (int i = 0; i < 20; ++i) p1.step();

  uint32_t v3 = p1.regs().read(3);
  uint32_t v4 = p1.regs().read(4);
  if (v3 != 0xFFFFFFFFu) { std::cerr << "LB sign-extend failed: got " << std::hex << v3 << "\n"; return 1; }
  if (v4 != 0x000000FFu) { std::cerr << "LBU zero-extend failed: got " << std::hex << v4 << "\n"; return 2; }

  // Test 2: SH then LH/LHU
  std::vector<uint32_t> prog2;
  prog2.push_back(encodeI(50, 0, 0x0, 1, 0x13)); // ADDI x1,x0,50
  prog2.push_back(encodeI(1, 0, 0x0, 2, 0x13));  // ADDI x2,x0,1
  prog2.push_back(encodeS(0, 2, 1, 0x0, 0x23)); // SB x2,0(x1)
  prog2.push_back(encodeI(128, 0, 0x0, 2, 0x13)); // ADDI x2,x0,128
  prog2.push_back(encodeS(1, 2, 1, 0x0, 0x23)); // SB x2,1(x1)
  prog2.push_back(encodeI(0, 1, 0x1, 3, 0x03)); // LH x3,0(x1)
  prog2.push_back(encodeI(0, 1, 0x5, 4, 0x03)); // LHU x4,0(x1)

  Pipeline p2(prog2);
  for (int i = 0; i < 25; ++i) p2.step();

  uint32_t vh = p2.regs().read(3);
  uint32_t vhu = p2.regs().read(4);
  if (vh != 0xFFFF8001u) { std::cerr << "LH sign-extend failed: got " << std::hex << vh << "\n"; return 3; }
  if (vhu != 0x00008001u) { std::cerr << "LHU zero-extend failed: got " << std::hex << vhu << "\n"; return 4; }

  std::cout << "pipeline load/store instr tests ok\n";
  return 0;
}
