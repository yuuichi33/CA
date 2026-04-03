#include <iostream>
#include <vector>
#include "cpu/pipeline.h"

using namespace cpu;

static uint32_t encodeI(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfff;
  return (uimm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

int main() {
  // create several NOPs (ADDI x0,x0,0)
  std::vector<uint32_t> prog;
  for (int i = 0; i < 16; ++i) prog.push_back(encodeI(0, 0, 0x0, 0, 0x13));

  Pipeline p(prog);

  // configure CSR and timer
  p.csr().write_mtvec(0x200);
  p.csr().set_mstatus_mie(true);
  p.csr().set_mie_timer(true);
  p.timer().set_interval(3);
  p.timer().set_enabled(true);

  // run a few cycles until interrupt should occur
  bool saw_intr = false;
  for (int i = 0; i < 10; ++i) {
    p.step();
    if (p.pc() == 0x200) { saw_intr = true; break; }
  }

  if (!saw_intr) {
    std::cerr << "timer interrupt not taken, pc=" << p.pc() << "\n";
    return 1;
  }

  std::cout << "timer interrupt handled, pc=" << p.pc() << std::endl;
  return 0;
}
