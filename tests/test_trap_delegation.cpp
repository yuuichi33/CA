#include <iostream>
#include <vector>
#include "cpu/pipeline.h"

using namespace cpu;

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

static uint32_t encodeU(uint32_t imm20, uint32_t rd, uint32_t opcode) {
  return (imm20 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeCSR(uint32_t csr, uint32_t rs1, uint32_t funct3, uint32_t rd) {
  return ((csr & 0xfff) << 20) | ((rs1 & 0x1f) << 15) | ((funct3 & 0x7) << 12) | ((rd & 0x1f) << 7) | 0x73u;
}

static uint32_t encodeSYSTEM(uint32_t funct12) {
  return (funct12 << 20) | 0x73u;
}

int main() {
  // Scenario 1: U-mode illegal CSR access (mstatus)
  {
    std::vector<uint32_t> prog;
    // attempt to CSRRW x1, mstatus, x0  (should be illegal in U-mode)
    prog.push_back(encodeCSR(0x300, 0, 0x1, 1));

    Pipeline p(prog);
    // map program page with user-executable bit so U-mode can fetch
    uint32_t prog_bytes = static_cast<uint32_t>(prog.size() * 4);
    const uint32_t page = 0x1000u;
    for (uint32_t off = 0; off < prog_bytes; off += page) {
      if (!p.mmu_map_4k(off, off, 0x9u | (1u << 4))) { std::cerr << "mmu_map_4k(program) failed\n"; return 1; }
    }
    // run in U-mode
    p.csr().set_privilege(0);
    for (int i = 0; i < 10; ++i) p.step();
    uint32_t mcause = p.csr().read(0x342);
    if (mcause != 2u) { std::cerr << "U-mode CSR access did not trap as illegal, mcause=" << mcause << "\n"; return 2; }
  }

  // Scenario 2: delegate page fault to S-mode
  {
    uint32_t vaddr = 0x40005000u;
    std::vector<uint32_t> prog;
    uint32_t hi = (vaddr + 0x800u) >> 12u;
    int32_t lo = static_cast<int32_t>(vaddr) - static_cast<int32_t>(hi << 12);
    prog.push_back(encodeU(hi, 1, 0x37));
    prog.push_back(encodeI(lo, 1, 0x0, 1, 0x13));
    prog.push_back(encodeI(0, 1, 0x2, 2, 0x03)); // LW x2,0(x1)

    Pipeline p(prog);
    // map program pages for S-mode fetch
    uint32_t prog_bytes = static_cast<uint32_t>(prog.size() * 4);
    const uint32_t page = 0x1000u;
    for (uint32_t off = 0; off < prog_bytes; off += page) {
      if (!p.mmu_map_4k(off, off, 0x9u)) { std::cerr << "mmu_map_4k(program) failed\n"; return 3; }
    }
    // delegate load page-fault (exception 13) to S-mode
    p.csr().write(0x302, (1u << 13u));
    // set stvec to safe handler
    p.csr().write(0x105, 0x2000u);
    // place a NOP at stvec and map it
    uint32_t nop = 0x00000013u;
    p.memory().store_bytes(0x2000u, reinterpret_cast<const uint8_t*>(&nop), 4);
    if (!p.mmu_map_4k(0x2000u, 0x2000u, 0x9u)) { std::cerr << "mmu_map_4k(stvec) failed\n"; return 3; }
    // set to S-mode
    p.csr().set_privilege(1);
    // run steps; LW should fault and be delegated to S-mode
    for (int i = 0; i < 20; ++i) p.step();
    uint32_t scause = p.csr().read(0x142);
    uint32_t sepc = p.csr().read(0x141);
    if (scause != 13u) { std::cerr << "Delegated trap not delivered to S-mode scause=" << scause << "\n"; return 4; }
    if (p.csr().get_privilege() != 1) { std::cerr << "Privilege not S after delegation, got " << p.csr().get_privilege() << "\n"; return 5; }
  }

  // Scenario 3: nested interrupt / MPIE save and restore
  {
    std::vector<uint32_t> prog;
    prog.push_back(0x00000013u); // NOP
    Pipeline p(prog);
    // map program page and mtvec handler containing MRET
    uint32_t prog_bytes = static_cast<uint32_t>(prog.size() * 4);
    const uint32_t page = 0x1000u;
    for (uint32_t off = 0; off < prog_bytes; off += page) {
      if (!p.mmu_map_4k(off, off, 0x9u | (1u << 4))) { std::cerr << "mmu_map_4k(program) failed\n"; return 6; }
    }
    // install mtvec -> handler at 0x1000 containing MRET
    p.csr().write(0x305, 0x1000u);
    uint32_t mret = encodeSYSTEM(0x302);
    p.memory().store_bytes(0x1000u, reinterpret_cast<const uint8_t*>(&mret), 4);
    if (!p.mmu_map_4k(0x1000u, 0x1000u, 0x9u)) { std::cerr << "mmu_map_4k(mtvec) failed\n"; return 6; }
    // enable machine interrupts and timer interrupt
    p.csr().set_mstatus_mie(true);
    p.csr().set_mie_timer(true);
    p.csr().set_mip_timer(true);
    // step once to take the interrupt
    p.step();
    // after trap entry: MPIE should contain old MIE (true) and MIE should be cleared
    if (!p.csr().get_mpie()) { std::cerr << "MPIE not set after trap\n"; return 7; }
    if (p.csr().get_mstatus_mie()) { std::cerr << "MIE not cleared after trap\n"; return 8; }
    // execute handler (MRET) to return
    for (int i = 0; i < 5; ++i) p.step();
    if (!p.csr().get_mstatus_mie()) { std::cerr << "MIE not restored after MRET\n"; return 9; }
  }

  std::cout << "trap delegation tests ok\n";
  return 0;
}
