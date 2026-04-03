#include "isa/decoder.h"
#include <cstdint>
#include <string>

namespace isa {

static inline int32_t sign_extend(uint32_t val, unsigned bits) {
  uint32_t m = 1u << (bits - 1);
  return static_cast<int32_t>((val ^ m) - m);
}

static inline uint32_t opcode_of(uint32_t inst) { return inst & 0x7f; }

Decoded decode(uint32_t inst) {
  Decoded d;
  d.opcode = opcode_of(inst);
  d.rd = (inst >> 7) & 0x1f;
  d.funct3 = (inst >> 12) & 0x7;
  d.rs1 = (inst >> 15) & 0x1f;
  d.rs2 = (inst >> 20) & 0x1f;
  d.funct7 = (inst >> 25) & 0x7f;

  switch (d.opcode) {
    case 0x33: // R-type
      d.type = InstType::R;
      // map common R-type names
      if (d.funct3 == 0x0 && d.funct7 == 0x00) d.name = "ADD";
      else if (d.funct3 == 0x0 && d.funct7 == 0x20) d.name = "SUB";
      else if (d.funct3 == 0x1) d.name = "SLL";
      else if (d.funct3 == 0x2) d.name = "SLT";
      else if (d.funct3 == 0x3) d.name = "SLTU";
      else if (d.funct3 == 0x4) d.name = "XOR";
      else if (d.funct3 == 0x5 && d.funct7 == 0x00) d.name = "SRL";
      else if (d.funct3 == 0x5 && d.funct7 == 0x20) d.name = "SRA";
      else if (d.funct3 == 0x6) d.name = "OR";
      else if (d.funct3 == 0x7) d.name = "AND";
      break;

    case 0x13: // I-type arithmetic
      d.type = InstType::I;
      d.imm = sign_extend(inst >> 20, 12);
      if (d.funct3 == 0x0) d.name = "ADDI";
      else if (d.funct3 == 0x1) d.name = "SLLI";
      else if (d.funct3 == 0x2) d.name = "SLTI";
      else if (d.funct3 == 0x3) d.name = "SLTIU";
      else if (d.funct3 == 0x4) d.name = "XORI";
      else if (d.funct3 == 0x5) d.name = "SRLI/SRAI";
      else if (d.funct3 == 0x6) d.name = "ORI";
      else if (d.funct3 == 0x7) d.name = "ANDI";
      break;

    case 0x03: // Loads (I-type)
      d.type = InstType::I;
      d.imm = sign_extend(inst >> 20, 12);
      if (d.funct3 == 0x0) d.name = "LB";
      else if (d.funct3 == 0x1) d.name = "LH";
      else if (d.funct3 == 0x2) d.name = "LW";
      else if (d.funct3 == 0x4) d.name = "LBU";
      else if (d.funct3 == 0x5) d.name = "LHU";
      break;

    case 0x23: { // Stores (S-type)
      d.type = InstType::S;
      uint32_t imm4_0 = (inst >> 7) & 0x1f;
      uint32_t imm11_5 = (inst >> 25) & 0x7f;
      uint32_t imm = (imm11_5 << 5) | imm4_0;
      d.imm = sign_extend(imm, 12);
      if (d.funct3 == 0x0) d.name = "SB";
      else if (d.funct3 == 0x1) d.name = "SH";
      else if (d.funct3 == 0x2) d.name = "SW";
    } break;

    case 0x63: { // Branches (B-type)
      d.type = InstType::B;
      uint32_t imm11 = (inst >> 7) & 0x1;
      uint32_t imm4_1 = (inst >> 8) & 0xf;
      uint32_t imm10_5 = (inst >> 25) & 0x3f;
      uint32_t imm12 = (inst >> 31) & 0x1;
      uint32_t imm = (imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) | (imm4_1 << 1);
      d.imm = sign_extend(imm, 13);
      if (d.funct3 == 0x0) d.name = "BEQ";
      else if (d.funct3 == 0x1) d.name = "BNE";
      else if (d.funct3 == 0x4) d.name = "BLT";
      else if (d.funct3 == 0x5) d.name = "BGE";
      else if (d.funct3 == 0x6) d.name = "BLTU";
      else if (d.funct3 == 0x7) d.name = "BGEU";
    } break;

    case 0x37: // LUI (U-type)
      d.type = InstType::U;
      d.imm = static_cast<int32_t>(inst & 0xfffff000);
      d.name = "LUI";
      break;

    case 0x17: // AUIPC (U-type)
      d.type = InstType::U;
      d.imm = static_cast<int32_t>(inst & 0xfffff000);
      d.name = "AUIPC";
      break;

    case 0x6f: // JAL (J-type)
      d.type = InstType::J;
      {
        uint32_t imm20 = (inst >> 31) & 0x1;
        uint32_t imm10_1 = (inst >> 21) & 0x3ff;
        uint32_t imm11 = (inst >> 20) & 0x1;
        uint32_t imm19_12 = (inst >> 12) & 0xff;
        uint32_t imm = (imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) | (imm10_1 << 1);
        d.imm = sign_extend(imm, 21);
        d.name = "JAL";
      }
      break;

    case 0x67: // JALR (I-type)
      d.type = InstType::I;
      d.imm = sign_extend(inst >> 20, 12);
      d.name = "JALR";
      break;

    case 0x73: // SYSTEM
      d.type = InstType::I;
      d.imm = sign_extend(inst >> 20, 12);
        {
          uint32_t funct12 = (inst >> 20) & 0xfff;
          if (inst == 0x00000073) d.name = "ECALL";
          else if (funct12 == 0x302) d.name = "MRET";
          else d.name = "SYSTEM";
        }
      break;

    default:
      d.type = InstType::OTHER;
      d.name = "UNKNOWN";
      break;
  }

  return d;
}

} // namespace isa
