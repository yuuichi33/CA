#include <iostream>
#include <cstdint>
#include "isa/decoder.h"

using namespace isa;

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

static uint32_t encodeB(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0x1fff; // 13 bits
  uint32_t imm12 = (uimm >> 12) & 0x1;
  uint32_t imm11 = (uimm >> 11) & 0x1;
  uint32_t imm10_5 = (uimm >> 5) & 0x3f;
  uint32_t imm4_1 = (uimm >> 1) & 0xf;
  return (imm12 << 31) | (imm11 << 7) | (imm10_5 << 25) | (imm4_1 << 8) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | opcode;
}

static uint32_t encodeSYSTEM(uint32_t funct12) {
  return (funct12 << 20) | 0x73u;
}

int main() {
  bool ok = true;

  // ADD x1, x2, x3
  uint32_t inst_add = encodeR(0x00, 3, 2, 0x0, 1, 0x33);
  auto dadd = decode(inst_add);
  if (dadd.type != InstType::R || dadd.name != "ADD" || dadd.rd != 1 || dadd.rs1 != 2 || dadd.rs2 != 3) {
    std::cerr << "ADD decode failed" << std::endl;
    ok = false;
  } else std::cout << "ADD ok" << std::endl;

  // SUB x5, x2, x3
  uint32_t inst_sub = encodeR(0x20, 3, 2, 0x0, 5, 0x33);
  auto dsub = decode(inst_sub);
  if (dsub.name != "SUB") { std::cerr << "SUB decode failed" << std::endl; ok = false; } else std::cout << "SUB ok" << std::endl;

  // ADDI x4, x5, 10
  uint32_t inst_addi = encodeI(10, 5, 0x0, 4, 0x13);
  auto daddi = decode(inst_addi);
  if (daddi.type != InstType::I || daddi.name != "ADDI" || daddi.imm != 10) { std::cerr << "ADDI failed" << std::endl; ok = false; } else std::cout << "ADDI ok" << std::endl;

  // LW x6, 16(x7)
  uint32_t inst_lw = encodeI(16, 7, 0x2, 6, 0x03);
  auto dlw = decode(inst_lw);
  if (dlw.name != "LW" || dlw.rs1 != 7 || dlw.rd != 6 || dlw.imm != 16) { std::cerr << "LW failed" << std::endl; ok = false; } else std::cout << "LW ok" << std::endl;

  // SW x6, 8(x7)
  uint32_t inst_sw = encodeS(8, 6, 7, 0x2, 0x23);
  auto dsw = decode(inst_sw);
  if (dsw.name != "SW" || dsw.rs1 != 7 || dsw.rs2 != 6 || dsw.imm != 8) { std::cerr << "SW failed" << std::endl; ok = false; } else std::cout << "SW ok" << std::endl;

  // BEQ x1, x2, 12
  uint32_t inst_beq = encodeB(12, 2, 1, 0x0, 0x63);
  auto dbeq = decode(inst_beq);
  if (dbeq.name != "BEQ" || dbeq.rs1 != 1 || dbeq.rs2 != 2 || dbeq.imm != 12) { std::cerr << "BEQ failed" << std::endl; ok = false; } else std::cout << "BEQ ok" << std::endl;

  // SRET (SYSTEM funct12=0x102)
  uint32_t inst_sret = encodeSYSTEM(0x102);
  auto dsret = decode(inst_sret);
  if (dsret.name != "SRET") { std::cerr << "SRET decode failed" << std::endl; ok = false; } else std::cout << "SRET ok" << std::endl;

  return ok ? 0 : 1;
}
