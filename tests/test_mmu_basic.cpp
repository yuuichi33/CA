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

int main() {
  // choose a virtual page (page-aligned) and a physical page (we'll let the pipeline allocate pages)
  uint32_t vaddr = 0x40002000u; // virtual address to map
  uint32_t val = 0x123u;

  // build program: LUI/ADDI to form vaddr in x1, ADDI x2,x0,val, SW x2,0(x1), LW x3,0(x1)
  std::vector<uint32_t> prog;
  uint32_t hi = (vaddr + 0x800u) >> 12u;
  int32_t lo = static_cast<int32_t>(vaddr) - static_cast<int32_t>(hi << 12);
  prog.push_back(encodeU(hi, 1, 0x37)); // LUI x1,hi
  prog.push_back(encodeI(lo, 1, 0x0, 1, 0x13)); // ADDI x1,x1,lo
  prog.push_back(encodeI(static_cast<int32_t>(val), 0, 0x0, 2, 0x13)); // ADDI x2,x0,val
  prog.push_back(encodeS(0, 2, 1, 0x2, 0x23)); // SW x2,0(x1)
  prog.push_back(encodeI(0, 1, 0x2, 3, 0x03)); // LW x3,0(x1)

  Pipeline p(prog);

  // enable verbose to see store/load debug messages
  p.set_verbose(true);

  // choose a physical address beyond current mem size
  uint32_t paddr = static_cast<uint32_t>(p.memory().size()) + 0x1000u;
  // map vaddr -> paddr with flags V|R|W (0b111)
  // also map program pages (identity) so instruction fetches succeed under MMU
  uint32_t prog_bytes = static_cast<uint32_t>(prog.size() * 4);
  const uint32_t page = 0x1000u;
  for (uint32_t off = 0; off < prog_bytes; off += page) {
    if (!p.mmu_map_4k(off, off, 0x9u)) { std::cerr << "mmu_map_4k(program) failed\n"; return 1; }
  }

  if (!p.mmu_map_4k(vaddr, paddr, 0x7u)) { std::cerr << "mmu_map_4k failed\n"; return 1; }

  // debug: verify page table entries we just wrote
  {
    uint32_t satp = p.csr().read(0x180);
    uint32_t root_ppn = satp & 0x003fffffu;
    uint32_t root_base = root_ppn << 12u;
    uint32_t vpn1 = (vaddr >> 22) & 0x3ffu;
    uint32_t vpn0 = (vaddr >> 12) & 0x3ffu;
    uint32_t pte1 = p.memory().load32(root_base + vpn1 * 4u);
    uint32_t next_ppn = pte1 >> 10u;
    uint32_t level0_base = next_ppn << 12u;
    uint32_t pte0 = p.memory().load32(level0_base + vpn0 * 4u);
    std::cerr << "DBG: satp=0x" << std::hex << satp << " root_base=0x" << root_base << " pte1=0x" << pte1 << " pte0=0x" << pte0 << std::dec << "\n";
  }

  // run
  for (int i = 0; i < 100; ++i) p.step();

  uint32_t got = p.regs().read(3);
  uint32_t phys_v = p.memory().load32(paddr);
  std::cerr << "DBG: reg x3=0x" << std::hex << got << " phys[0x" << paddr << "]=0x" << phys_v << std::dec << "\n";
  if (got != val) { std::cerr << "MMU mapping load/store failed: got " << std::hex << got << " expected " << val << "\n"; return 2; }
  if (phys_v != val) { std::cerr << "Physical memory not written: got " << std::hex << phys_v << " expected " << val << "\n"; return 3; }

  std::cout << "mmu basic test ok\n";
  return 0;
}
