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
  std::string name;
};

Decoded decode(uint32_t inst);

} // namespace isa
