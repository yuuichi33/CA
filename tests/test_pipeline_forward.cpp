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

int main() {
  // Program: ADDI x1, x0, 5 ; ADD x2, x1, x1
  std::vector<uint32_t> prog;
  prog.push_back(encodeI(5, 0, 0x0, 1, 0x13));
  prog.push_back(encodeR(0x00, 1, 1, 0x0, 2, 0x33));

  Pipeline p(prog);

  // Run for several cycles to flush through pipeline
  for (int i = 0; i < 6; ++i) p.step();

  uint32_t x2 = p.regs().read(2);
  if (x2 != 10) {
    std::cerr << "forwarding failed: x2=" << x2 << " expected 10\n";
    return 1;
  }
  std::cout << "forwarding ok, x2=" << x2 << std::endl;
  return 0;
}
