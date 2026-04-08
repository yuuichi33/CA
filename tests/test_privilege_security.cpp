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

static uint32_t encodeSYSTEM(uint32_t funct12) {
  return (funct12 << 20) | 0x73u;
}

int main() {
  // Test 1: U-mode cannot access kernel page (U=0)
  {
    uint32_t vaddr = 0x40003000u;
    uint32_t val = 0xdeadbeefu;
    std::vector<uint32_t> prog;
    // build a simple program that writes then reads
    uint32_t hi = (vaddr + 0x800u) >> 12u;
    int32_t lo = static_cast<int32_t>(vaddr) - static_cast<int32_t>(hi << 12);
    prog.push_back(encodeU(hi, 1, 0x37)); // LUI x1,hi
    prog.push_back(encodeI(lo, 1, 0x0, 1, 0x13)); // ADDI x1,x1,lo
    prog.push_back(encodeI(static_cast<int32_t>(val), 0, 0x0, 2, 0x13)); // ADDI x2,x0,val
    prog.push_back(encodeS(0, 2, 1, 0x2, 0x23)); // SW x2,0(x1)
    prog.push_back(encodeI(0, 1, 0x2, 3, 0x03)); // LW x3,0(x1)

    Pipeline p(prog);
    // map program pages identity so instruction fetches work under MMU
    uint32_t prog_bytes = static_cast<uint32_t>(prog.size() * 4);
    const uint32_t page = 0x1000u;
    for (uint32_t off = 0; off < prog_bytes; off += page) {
      if (!p.mmu_map_4k(off, off, 0x9u | (1u << 4))) { std::cerr << "mmu_map_4k(program) failed\n"; return 1; }
    }
    std::cerr << "Test1: program pages mapped, prog_bytes=" << prog_bytes << "\n";
    // map vaddr -> paddr with U=0 (kernel page)
    uint32_t paddr = static_cast<uint32_t>(p.memory().size()) + 0x1000u;
    if (!p.mmu_map_4k(vaddr, paddr, 0x7u)) { std::cerr << "mmu_map_4k failed\n"; return 1; }
    std::cerr << "Test1: data vaddr mapped vaddr=0x" << std::hex << vaddr << " paddr=0x" << paddr << std::dec << "\n";
    // set to U-mode
    p.csr().set_privilege(0);
    // run
    for (int i = 0; i < 100; ++i) p.step();
    uint32_t mcause = p.csr().read(0x342);
    if (mcause != 15u) { std::cerr << "U-mode access did not fault as expected, mcause=" << mcause << "\n"; return 2; }
  }

  // Test 2: S-mode with SUM=0 cannot access user page (U=1)
  {
    uint32_t vaddr = 0x40004000u;
    uint32_t val = 0xbeefu;
    std::vector<uint32_t> prog;
    uint32_t hi = (vaddr + 0x800u) >> 12u;
    int32_t lo = static_cast<int32_t>(vaddr) - static_cast<int32_t>(hi << 12);
    prog.push_back(encodeU(hi, 1, 0x37));
    prog.push_back(encodeI(lo, 1, 0x0, 1, 0x13));
    prog.push_back(encodeI(static_cast<int32_t>(val), 0, 0x0, 2, 0x13));
    prog.push_back(encodeS(0, 2, 1, 0x2, 0x23));

    Pipeline p(prog);
    // map program pages identity so instruction fetches work under MMU
    uint32_t prog_bytes = static_cast<uint32_t>(prog.size() * 4);
    const uint32_t page = 0x1000u;
    for (uint32_t off = 0; off < prog_bytes; off += page) {
      if (!p.mmu_map_4k(off, off, 0x9u)) { std::cerr << "mmu_map_4k(program) failed\n"; return 3; }
    }
    std::cerr << "Test2: program pages mapped, prog_bytes=" << prog_bytes << "\n";
    // map vaddr -> paddr with U=1 (user page)
    uint32_t paddr = static_cast<uint32_t>(p.memory().size()) + 0x2000u;
    if (!p.mmu_map_4k(vaddr, paddr, 0x7u | (1u << 4))) { std::cerr << "mmu_map_4k failed\n"; return 3; }
    std::cerr << "Test2: data vaddr mapped vaddr=0x" << std::hex << vaddr << " paddr=0x" << paddr << std::dec << "\n";
    // set to S-mode and ensure SUM=0
    p.csr().set_privilege(1);
    p.csr().set_sum(false);
    for (int i = 0; i < 100; ++i) p.step();
    uint32_t mcause = p.csr().read(0x342);
    if (mcause != 15u && mcause != 13u) { std::cerr << "S-mode SUM access did not fault as expected, mcause=" << mcause << "\n"; return 4; }
  }

  // Test 3: M-mode -> MRET to U-mode -> ECALL returns to M-mode
  {
    // Program: MRET at 0, ECALL at 4
    std::vector<uint32_t> prog;
    prog.push_back(encodeSYSTEM(0x302)); // MRET
    prog.push_back(0x00000073u); // ECALL

    Pipeline p(prog);
    // map program page identity for MRET/ECALL sequence
    uint32_t prog_bytes = static_cast<uint32_t>(prog.size() * 4);
    const uint32_t page = 0x1000u;
    for (uint32_t off = 0; off < prog_bytes; off += page) {
      if (!p.mmu_map_4k(off, off, 0x9u | (1u << 4))) { std::cerr << "mmu_map_4k(program) failed\n"; return 5; }
    }
    std::cerr << "Test3: program pages mapped, prog_bytes=" << prog_bytes << "\n";
    // set mepc to 4 so MRET will jump to ECALL
    p.csr().write_mepc(4);
    // ensure MPP=0 so MRET will set U-mode
    p.csr().set_mpp(0);
    // set mtvec to a safe handler (avoid pointing to 0 which contains MRET)
    p.csr().write(0x305, 0x1000u);
    // allocate a NOP at mtvec and map it so the trap handler won't immediately MRET
    uint32_t nop = 0x00000013u; // ADDI x0,x0,0
    p.memory().store_bytes(0x1000u, reinterpret_cast<const uint8_t*>(&nop), 4);
    if (!p.mmu_map_4k(0x1000u, 0x1000u, 0x9u)) { std::cerr << "mmu_map_4k(mtvec) failed\n"; return 5; }
    // run enough steps and log each step for debugging
    p.set_verbose(true);
    for (int i = 0; i < 10; ++i) {
      p.step();
      std::cerr << "step=" << i << " pc=0x" << std::hex << p.pc() << " priv=" << std::dec << p.csr().get_privilege() << " mcause=" << p.csr().read(0x342) << "\n";
    }
    // after MRET, the ECALL should execute and trap back to M-mode
    uint32_t mcause = p.csr().read(0x342);
    if (mcause != 8u) { std::cerr << "ECALL from U-mode did not produce expected mcause=8, got " << mcause << "\n"; return 5; }
    if (p.csr().get_privilege() != 3) { std::cerr << "Privilege not restored to M-mode after ECALL, got " << p.csr().get_privilege() << "\n"; return 6; }
  }

  std::cout << "privilege security tests ok\n";
  return 0;
}
