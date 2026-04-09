#include <iostream>
#include <vector>
#include "cpu/pipeline.h"
#include "isa/decoder.h"
#include "cache/cache_config.h"

using namespace cpu;
using namespace isa;

static uint32_t encodeR(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeI(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfff;
  return (uimm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t encodeB(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0x1fff;
  uint32_t imm12 = (uimm >> 12) & 0x1;
  uint32_t imm10_5 = (uimm >> 5) & 0x3f;
  uint32_t imm4_1 = (uimm >> 1) & 0xf;
  uint32_t imm11 = (uimm >> 11) & 0x1;
  return (imm12 << 31) | (imm10_5 << 25) | (rs2 << 20) | (rs1 << 15) |
         (funct3 << 12) | (imm4_1 << 8) | (imm11 << 7) | opcode;
}

int main() {
  // Program: LW x1, 0(x0) ; ADD x2, x1, x3
  std::vector<uint32_t> prog;
  prog.push_back(encodeI(0, 0, 0x2, 1, 0x03)); // LW x1,0(x0)
  prog.push_back(encodeR(0x00, 3, 1, 0x0, 2, 0x33)); // ADD x2,x1,x3

  Pipeline p(prog);

  bool s1 = p.step(); // fetch LW
  bool s2 = p.step(); // move LW -> ID/EX, fetch ADD
  bool s3 = p.step(); // detect load-use hazard and insert bubble

  if (s1) { std::cerr << "unexpected stall on cycle1\n"; return 1; }
  if (s2) { std::cerr << "unexpected stall on cycle2\n"; return 1; }
  if (!s3) { std::cerr << "expected stall on cycle3 but none\n"; return 1; }
  if (p.idex().inst.name != "NOP") { std::cerr << "expected NOP in ID/EX after stall\n"; return 1; }

  // Cache miss latency should affect runtime cycles in pipeline fetch path.
  {
    std::vector<uint32_t> seq_prog(128, 0x00000013u); // NOP stream

    struct RunStats {
      uint64_t cycles = 0;
      uint64_t cache_stalls = 0;
    };

    auto run_with_miss_latency = [&](int miss_latency) -> RunStats {
      Pipeline q(seq_prog);
      CacheConfig cfg;
      cfg.cache_size = 4;
      cfg.line_size = 4;
      cfg.associativity = 1;
      cfg.write_back = true;
      cfg.write_allocate = true;
      cfg.miss_latency = miss_latency;
      q.configure_cache(true, cfg);

      int guard = 0;
      while (q.instrs() < 24 && guard < 20000) {
        q.step();
        ++guard;
      }
      if (q.instrs() < 24) return {};
      return {q.cycles(), q.cache_stall_cycles()};
    };

    RunStats s0 = run_with_miss_latency(0);
    RunStats s4 = run_with_miss_latency(4);
    if (s0.cycles == 0 || s4.cycles == 0) {
      std::cerr << "miss-latency benchmark did not retire enough instructions\n";
      return 1;
    }
    if (s4.cycles <= s0.cycles) {
      std::cerr << "expected higher cycles with miss_latency=4, got c0=" << s0.cycles << " c4=" << s4.cycles << "\n";
      return 1;
    }
    if (s4.cache_stalls <= s0.cache_stalls) {
      std::cerr << "expected higher cache stall cycles with miss_latency=4, got s0="
                << s0.cache_stalls << " s4=" << s4.cache_stalls << "\n";
      return 1;
    }
  }

  // A load miss stall must not let EX use stale rs values captured before an older WB update.
  {
    std::vector<uint32_t> prog;
    prog.push_back(encodeI(200, 0, 0x0, 14, 0x13));            // ADDI x14, x0, 200 (stale seed)
    prog.push_back(encodeI(0x100, 0, 0x0, 1, 0x13));           // ADDI x1,  x0, 0x100
    prog.push_back(encodeI(0, 1, 0x2, 12, 0x03));              // LW   x12, 0(x1)
    prog.push_back(encodeI(4, 1, 0x2, 14, 0x03));              // LW   x14, 4(x1)
    prog.push_back(encodeI(8, 1, 0x2, 15, 0x03));              // LW   x15, 8(x1) (can miss+stall)
    prog.push_back(encodeB(12, 12, 14, 0x5, 0x63));            // BGE  x14, x12, fail
    prog.push_back(encodeI(1, 0, 0x0, 10, 0x13));              // ADDI x10, x0, 1 (pass marker)
    prog.push_back(encodeB(8, 0, 0, 0x0, 0x63));               // BEQ  x0, x0, done
    prog.push_back(encodeI(2, 0, 0x0, 10, 0x13));              // fail: ADDI x10, x0, 2
    prog.push_back(0x0000006fu);                               // done: JAL x0, 0

    auto run_branch_case = [&](int miss_latency) -> uint32_t {
      Pipeline q(prog);
      CacheConfig cfg;
      cfg.cache_size = 16;
      cfg.line_size = 4;
      cfg.associativity = 1;
      cfg.write_back = true;
      cfg.write_allocate = true;
      cfg.miss_latency = miss_latency;
      q.configure_cache(true, cfg);

      q.memory().store32(0x100, 100u);
      q.memory().store32(0x104, 50u);
      q.memory().store32(0x108, 7u);

      int guard = 0;
      while (guard < 500 && q.regs().read(10) == 0) {
        q.step();
        ++guard;
      }
      return q.regs().read(10);
    };

    uint32_t a0_m0 = run_branch_case(0);
    uint32_t a0_m1 = run_branch_case(1);
    if (a0_m0 != 1u || a0_m1 != 1u) {
      std::cerr << "stalled-branch forwarding regression: miss0 a0=" << a0_m0
                << " miss1 a0=" << a0_m1 << "\n";
      return 1;
    }
  }

  std::cout << "pipeline hazard detection ok" << std::endl;
  return 0;
}
