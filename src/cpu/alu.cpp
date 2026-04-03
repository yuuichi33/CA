#include "cpu/alu.h"

namespace cpu {

uint32_t alu_exec(ALUOp op, uint32_t a, uint32_t b) {
  switch (op) {
    case ALUOp::ADD: return a + b;
    case ALUOp::SUB: return a - b;
    case ALUOp::SLL: return a << (b & 0x1f);
    case ALUOp::SLT: return (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? 1u : 0u;
    case ALUOp::SLTU: return (a < b) ? 1u : 0u;
    case ALUOp::XOR: return a ^ b;
    case ALUOp::SRL: return a >> (b & 0x1f);
    case ALUOp::SRA: return static_cast<uint32_t>(static_cast<int32_t>(a) >> (b & 0x1f));
    case ALUOp::OR: return a | b;
    case ALUOp::AND: return a & b;
  }
  return 0u;
}

} // namespace cpu
