#pragma once
#include "isa/decoder.h"
#include "cpu/registers.h"
#include "mem/memory.h"
#include "cache/simple_cache.h"
#include <cstdint>
#include <string>
#include <vector>

namespace cpu {

struct IFIDReg {
  isa::Decoded inst;
  uint32_t pc = 0;
  bool valid = false;
};

struct IDEXReg {
  isa::Decoded inst;
  uint32_t pc = 0;
  bool valid = false;
  uint32_t rs1_val = 0;
  uint32_t rs2_val = 0;
};

struct EXMEMReg {
  isa::Decoded inst;
  uint32_t pc = 0;
  bool valid = false;
  uint32_t alu_result = 0;
  uint32_t rs2_val = 0; // for stores
  uint32_t rd = 0;
  bool write_back = false;
};

struct MEMWBReg {
  isa::Decoded inst;
  uint32_t pc = 0;
  bool valid = false;
  uint32_t alu_result = 0;
  uint32_t mem_data = 0;
  uint32_t rd = 0;
  bool write_back = false;
};

class Pipeline {
public:
  explicit Pipeline(const std::vector<uint32_t>& program = {}, size_t mem_size = 64*1024);
  void reset();
  // step returns true if a stall/bubble was inserted this cycle
  bool step();
  uint32_t pc() const { return pc_; }
  const IFIDReg& ifid() const { return ifid_; }
  const IDEXReg& idex() const { return idex_; }
  const EXMEMReg& exmem() const { return exmem_; }
  const MEMWBReg& memwb() const { return memwb_; }
  RegisterFile& regs() { return regs_; }
  std::string dump_regs() const;

private:
  std::vector<uint32_t> program_;
  uint32_t pc_ = 0;
  IFIDReg ifid_;
  IDEXReg idex_;
  EXMEMReg exmem_;
  MEMWBReg memwb_;
  RegisterFile regs_;
  mem::Memory mem_;
  // caches
  SimpleCache* icache_ = nullptr;
  SimpleCache* dcache_ = nullptr;
  // stall simulation for cache misses
  int stall_counter_ = 0;
  enum class StallKind { None, If, MemLoad };
  StallKind stall_kind_ = StallKind::None;
  uint32_t pending_if_word_ = 0;
  uint32_t pending_if_pc_ = 0;
  MEMWBReg pending_memwb_;
  uint32_t pending_mem_value_ = 0;
  bool last_stall_ = false;
};

} // namespace cpu
