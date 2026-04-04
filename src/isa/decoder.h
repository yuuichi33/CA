#pragma once
#include <cstdint>
#include <string>

namespace isa {

enum class InstType { R, I, S, B, U, J, OTHER };

struct Decoded {
  InstType type = InstType::OTHER;
  uint32_t opcode = 0;
  uint32_t rd = 0;
  uint32_t rs1 = 0;
  uint32_t rs2 = 0;
  uint32_t funct3 = 0;
  uint32_t funct7 = 0;
  int32_t imm = 0;
  uint32_t csr = 0; // CSR address for SYSTEM/CSR instructions (bits 31:20)
  uint32_t zimm = 0; // zero-extended immediate for CSR immediate variants (rs1 field)
  std::string name;
};

Decoded decode(uint32_t inst);

} // namespace isa
