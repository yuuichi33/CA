#include <iostream>
#include <vector>
#include "cpu/pipeline.h"

using namespace cpu;

static uint32_t encodeCSR(uint32_t csr, uint32_t rs1, uint32_t funct3, uint32_t rd) {
  return ((csr & 0xfff) << 20) | ((rs1 & 0x1f) << 15) | ((funct3 & 0x7) << 12) | ((rd & 0x1f) << 7) | 0x73u;
}

int main() {
  std::vector<uint32_t> prog;
  // CSRRWI x1, 0x300, 8  ; write mstatus = 8 into CSR, return old into x1
  prog.push_back(encodeCSR(0x300, 8, 0x5, 1));
  // CSRRW x2, 0x300, x0 ; swap CSR with x0 (0), x2 <- old value
  prog.push_back(encodeCSR(0x300, 0, 0x1, 2));

  Pipeline p(prog);
  for (int i = 0; i < 30; ++i) p.step();

  uint32_t v2 = p.regs().read(2);
  if (v2 != 8u) { std::cerr << "CSRRWI/CSRRW failed: got " << v2 << "\n"; return 1; }
  uint32_t mstatus = p.csr().read(0x300);
  if (mstatus != 0u) { std::cerr << "CSR write not observed: mstatus=" << mstatus << "\n"; return 2; }

  std::cout << "csr instr tests ok\n";
  return 0;
}
