#include <iostream>
#include <vector>
#include "cpu/pipeline.h"
#include "isa/decoder.h"

using namespace cpu;
using namespace isa;

static uint32_t encodeR(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeI(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfff;
  return (uimm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

int main() {
  // Program: LW x1, 0(x0) ; ADD x2, x1, x3
  std::vector<uint32_t> prog;
  prog.push_back(encodeI(0, 0, 0x2, 1, 0x03)); // LW x1,0(x0)
  prog.push_back(encodeR(0x00, 3, 1, 0x0, 2, 0x33)); // ADD x2,x1,x3

  Pipeline p(prog);

  bool s1 = p.step(); // fetch LW
  bool s2 = p.step(); // move LW -> ID/EX, fetch ADD
  bool s3 = p.step(); // detect load-use hazard and insert bubble

  if (s1) { std::cerr << "unexpected stall on cycle1\n"; return 1; }
  if (s2) { std::cerr << "unexpected stall on cycle2\n"; return 1; }
  if (!s3) { std::cerr << "expected stall on cycle3 but none\n"; return 1; }
  if (p.idex().inst.name != "NOP") { std::cerr << "expected NOP in ID/EX after stall\n"; return 1; }

  std::cout << "pipeline hazard detection ok" << std::endl;
  return 0;
}
