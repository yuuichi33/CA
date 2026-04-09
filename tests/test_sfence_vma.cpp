#include <iostream>
#include <vector>
#include "cpu/pipeline.h"
#include "cache/cache_config.h"

using namespace cpu;

static uint32_t encodeI(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfffu;
  return (uimm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeU(uint32_t imm20, uint32_t rd, uint32_t opcode) {
  return (imm20 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeS(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfffu;
  uint32_t imm11_5 = (uimm >> 5) & 0x7fu;
  uint32_t imm4_0 = uimm & 0x1fu;
  return (imm11_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (imm4_0 << 7) | opcode;
}

static uint32_t encodeSYSTEM(uint32_t funct12) {
  return (funct12 << 20) | 0x73u;
}

int main() {
  // pick a virtual address to test
  const uint32_t V = 0x40005000u;
  const uint32_t VAL1 = 0x11111111u;
  const uint32_t VAL2 = 0x22222222u;

  std::vector<uint32_t> prog;
  uint32_t hi = (V + 0x800u) >> 12u;
  int32_t lo = static_cast<int32_t>(V) - static_cast<int32_t>(hi << 12);
  // LUI x1,hi; ADDI x1,x1,lo; LW x2,0(x1); 8*NOPs; LW x3,0(x1)
  prog.push_back(encodeU(hi, 1, 0x37));
  prog.push_back(encodeI(lo, 1, 0x0, 1, 0x13));
  prog.push_back(encodeI(0, 1, 0x2, 2, 0x03)); // LW x2,0(x1)
  for (int i = 0; i < 8; ++i) prog.push_back(0x00000013u); // NOP
  prog.push_back(encodeI(0, 1, 0x2, 3, 0x03)); // LW x3,0(x1)

  // Case A: remap without SFENCE.VMA -> second load sees stale translation
  {
    Pipeline p(prog);
    // map program pages identity for instruction fetch
    uint32_t prog_bytes = static_cast<uint32_t>(prog.size() * 4);
    const uint32_t page = 0x1000u;
    for (uint32_t off = 0; off < prog_bytes; off += page) {
      if (!p.mmu_map_4k(off, off, 0x9u)) { std::cerr << "mmu_map_4k(program) failed\n"; return 1; }
    }

    // initial mapping V -> P1
    uint32_t p1 = static_cast<uint32_t>(p.memory().size()) + 0x1000u;
    if (!p.mmu_map_4k(V, p1, 0x7u)) { std::cerr << "mmu_map_4k failed\n"; return 2; }
    p.memory().store32(p1, VAL1);

    // run until first load writes x2
    for (int i = 0; i < 500; ++i) { p.step(); if (p.regs().read(2) == VAL1) break; }
    if (p.regs().read(2) != VAL1) { std::cerr << "first load did not complete as expected\n"; return 3; }

    // confirm TLB populated
    if (p.tlb_count() == 0) { std::cerr << "TLB not populated after first load\n"; return 4; }

    // remap V -> P2 (overwrite PTE). Pick P2 after P1 to avoid overlapping allocations.
    uint32_t p2 = p1 + 0x1000u;
    if (!p.mmu_map_4k(V, p2, 0x7u)) { std::cerr << "mmu_map_4k remap failed\n"; return 5; }
    p.memory().store32(p2, VAL2);

    // without SFENCE, TLB may still contain entries (implementation-dependent)
    if (p.tlb_count() == 0) { std::cerr << "expected TLB entries after remap (without sfence)\n"; return 6; }

    // perform global SFENCE.VMA (vaddr=0, asid=0) -> clears all TLB entries
    p.sfence_vma(0u, 0u);
    if (p.tlb_count() != 0) { std::cerr << "sfence_vma(0,0) did not flush TLB, count=" << p.tlb_count() << "\n"; return 7; }

    // continue and ensure next load uses new mapping
    for (int i = 0; i < 500; ++i) { p.step(); if (p.regs().read(3) != 0) break; }
    if (p.regs().read(3) == 0) { std::cerr << "second load did not complete\n"; return 8; }
    if (p.regs().read(3) != VAL2) { std::cerr << "after sfence: expected VAL2, got 0x" << std::hex << p.regs().read(3) << std::dec << "\n"; return 9; }
  }

  // Case B: remap then SFENCE.VMA (by VPN) -> second load sees new mapping
  {
    Pipeline p(prog);
    uint32_t prog_bytes = static_cast<uint32_t>(prog.size() * 4);
    const uint32_t page = 0x1000u;
    for (uint32_t off = 0; off < prog_bytes; off += page) {
      if (!p.mmu_map_4k(off, off, 0x9u)) { std::cerr << "mmu_map_4k(program) failed\n"; return 7; }
    }

    uint32_t p1 = static_cast<uint32_t>(p.memory().size()) + 0x1000u;
    if (!p.mmu_map_4k(V, p1, 0x7u)) { std::cerr << "mmu_map_4k failed\n"; return 8; }
    p.memory().store32(p1, VAL1);
    for (int i = 0; i < 500; ++i) { p.step(); if (p.regs().read(2) == VAL1) break; }
    if (p.regs().read(2) != VAL1) { std::cerr << "first load did not complete (case B)\n"; return 9; }

    uint32_t p2 = p1 + 0x1000u;
    if (!p.mmu_map_4k(V, p2, 0x7u)) { std::cerr << "mmu_map_4k remap failed\n"; return 10; }
    p.memory().store32(p2, VAL2);

    // perform SFENCE.VMA for this VPN (vaddr != 0, asid = 0 -> invalidate by VPN across ASIDs)
    p.sfence_vma(V, 0u);

    for (int i = 0; i < 500; ++i) { p.step(); if (p.regs().read(3) != 0) break; }
    if (p.regs().read(3) == 0) { std::cerr << "second load did not complete (case B)\n"; return 11; }
    if (p.regs().read(3) != VAL2) { std::cerr << "with sfence: expected VAL2, got 0x" << std::hex << p.regs().read(3) << std::dec << "\n"; return 12; }
  }

  // Case C: SFENCE.VMA instruction path must make dirty PTE updates visible under write-back D-cache.
  {
    Pipeline p(std::vector<uint32_t>{});

    CacheConfig cfg;
    cfg.cache_size = 64;
    cfg.line_size = 16;
    cfg.associativity = 1;
    cfg.write_back = true;
    cfg.write_allocate = true;
    cfg.miss_latency = 0;
    p.configure_cache(true, cfg);

    const uint32_t V3 = 0x40006000u;
    const uint32_t P1 = 0x3000u;
    const uint32_t P2 = 0x4000u;
    const uint32_t OLDV = 0x33333333u;
    const uint32_t NEWV = 0x44444444u;

    // Data pages contents.
    p.memory().store32(P1, OLDV);
    p.memory().store32(P2, NEWV);

    // Map code page and initial data mapping.
    if (!p.mmu_map_4k(0x00000000u, 0x00000000u, 0x9u)) { std::cerr << "mmu_map_4k(code) failed\n"; return 13; }
    if (!p.mmu_map_4k(V3, P1, 0x7u)) { std::cerr << "mmu_map_4k(V3->P1) failed\n"; return 14; }

    // Locate the leaf PTE for V3.
    uint32_t satp = p.csr().read(0x180);
    uint32_t root_base = (satp & 0x003fffffu) << 12u;
    uint32_t vpn1 = (V3 >> 22) & 0x3ffu;
    uint32_t vpn0 = (V3 >> 12) & 0x3ffu;
    uint32_t pte1_addr = root_base + vpn1 * 4u;
    uint32_t pte1 = p.memory().load32(pte1_addr);
    uint32_t level0_base = (pte1 >> 10u) << 12u;
    uint32_t pte0_addr = level0_base + vpn0 * 4u;

    // Program will store to pte0_addr virtually, so map that page identity.
    uint32_t pte_page = pte0_addr & ~0xfffu;
    if (!p.mmu_map_4k(pte_page, pte_page, 0x7u)) { std::cerr << "mmu_map_4k(pte page) failed\n"; return 15; }

    uint32_t new_pte = ((P2 >> 12u) << 10u) | 0x7u;

    auto hi20 = [](uint32_t x) -> uint32_t { return (x + 0x800u) >> 12u; };
    auto lo12 = [&](uint32_t x) -> int32_t {
      uint32_t hi = hi20(x);
      return static_cast<int32_t>(x) - static_cast<int32_t>(hi << 12u);
    };

    // Assemble program directly into memory at PC=0:
    //   x1 = pte0_addr
    //   x2 = new_pte
    //   sw x2,0(x1)
    //   sfence.vma x0,x0
    //   x3 = V3
    //   lw x4,0(x3)
    //   loop
    p.memory().store32(0x00u, encodeU(hi20(pte0_addr), 1, 0x37));
    p.memory().store32(0x04u, encodeI(lo12(pte0_addr), 1, 0x0, 1, 0x13));
    p.memory().store32(0x08u, encodeU(hi20(new_pte), 2, 0x37));
    p.memory().store32(0x0cu, encodeI(lo12(new_pte), 2, 0x0, 2, 0x13));
    p.memory().store32(0x10u, encodeS(0, 2, 1, 0x2, 0x23));
    p.memory().store32(0x14u, encodeSYSTEM(0x120)); // SFENCE.VMA x0, x0
    p.memory().store32(0x18u, encodeU(hi20(V3), 3, 0x37));
    p.memory().store32(0x1cu, encodeI(lo12(V3), 3, 0x0, 3, 0x13));
    p.memory().store32(0x20u, encodeI(0, 3, 0x2, 4, 0x03));
    p.memory().store32(0x24u, 0x0000006fu); // JAL x0,0

    for (int i = 0; i < 800; ++i) {
      p.step();
      if (p.regs().read(4) == NEWV) break;
    }
    if (p.regs().read(4) != NEWV) {
      std::cerr << "SFENCE instruction path did not expose updated PTE mapping, got 0x"
                << std::hex << p.regs().read(4) << " expected 0x" << NEWV << std::dec << "\n";
      return 16;
    }
  }

  std::cout << "sfence vma tests ok\n";
  return 0;
}
