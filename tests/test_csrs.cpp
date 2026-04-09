#include <iostream>
#include <vector>
#include "cpu/pipeline.h"

using namespace cpu;

static uint32_t encodeCSR(uint32_t csr, uint32_t rs1, uint32_t funct3, uint32_t rd) {
  return ((csr & 0xfff) << 20) | ((rs1 & 0x1f) << 15) | ((funct3 & 0x7) << 12) | ((rd & 0x1f) << 7) | 0x73u;
}

static uint32_t encodeSYSTEM(uint32_t funct12) {
  return (funct12 << 20) | 0x73u;
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

  // SRET: return from S-mode trap should jump to sepc and restore privilege.
  {
    std::vector<uint32_t> sret_prog;
    sret_prog.push_back(encodeSYSTEM(0x102)); // SRET
    sret_prog.push_back(0x0000006fu);         // JAL x0, 0 at sepc target
    Pipeline sret_pipe(sret_prog);

    // Prepare S-mode trap return context.
    sret_pipe.csr().write(0x141, 4u);      // sepc = 4
    sret_pipe.csr().set_privilege(1);      // currently in S-mode trap handler
    sret_pipe.csr().set_spp(false);        // return to U-mode
    sret_pipe.csr().set_spie(true);        // restore SIE to 1 after SRET
    sret_pipe.csr().set_mstatus_sie(false);

    bool sret_committed = false;
    for (int i = 0; i < 20; ++i) {
      sret_pipe.step();
      if (sret_pipe.csr().get_privilege() == 0) {
        sret_committed = true;
        break;
      }
    }

    if (!sret_committed) {
      std::cerr << "SRET did not complete within expected cycles\n";
      return 3;
    }
    if (sret_pipe.pc() != 4u) {
      std::cerr << "SRET did not jump to sepc target: pc=" << sret_pipe.pc() << "\n";
      return 3;
    }
    if (sret_pipe.csr().get_privilege() != 0) {
      std::cerr << "SRET did not restore to U-mode: privilege=" << sret_pipe.csr().get_privilege() << "\n";
      return 4;
    }
    if (!sret_pipe.csr().get_mstatus_sie()) {
      std::cerr << "SRET did not restore SIE from SPIE\n";
      return 5;
    }
  }

  // U-mode SRET should raise illegal instruction trap (mcause=2).
  {
    std::vector<uint32_t> bad_sret_prog;
    bad_sret_prog.push_back(encodeSYSTEM(0x102)); // SRET
    Pipeline bad_sret_pipe(bad_sret_prog);
    bad_sret_pipe.csr().set_privilege(0); // U-mode

    for (int i = 0; i < 8; ++i) bad_sret_pipe.step();
    uint32_t mcause = bad_sret_pipe.csr().read(0x342);
    if (mcause != 2u) {
      std::cerr << "U-mode SRET did not trap as illegal instruction, mcause=" << mcause << "\n";
      return 6;
    }
  }

  std::cout << "csr instr tests ok\n";
  return 0;
}
