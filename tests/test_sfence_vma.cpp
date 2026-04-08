#include <iostream>
#include <vector>
#include "cpu/pipeline.h"

using namespace cpu;

static uint32_t encodeI(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfffu;
  return (uimm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeU(uint32_t imm20, uint32_t rd, uint32_t opcode) {
  return (imm20 << 12) | (rd << 7) | opcode;
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

  std::cout << "sfence vma tests ok\n";
  return 0;
}
