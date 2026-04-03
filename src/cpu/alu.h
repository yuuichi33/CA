#pragma once
#include <cstdint>

namespace cpu {

enum class ALUOp { ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND };

uint32_t alu_exec(ALUOp op, uint32_t a, uint32_t b);

} // namespace cpu
