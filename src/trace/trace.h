#pragma once

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace trace {

struct StageState {
  bool valid = false;
  uint32_t pc = 0;
  std::string instr;
  uint32_t rd = 0;
  bool write_back = false;
  uint32_t alu_result = 0;
  uint32_t mem_data = 0;
};

struct RegWrite {
  uint32_t rd = 0;
  uint32_t value = 0;
};

struct MemAccess {
  std::string stage;
  std::string type;
  uint32_t vaddr = 0;
  uint32_t paddr = 0;
  uint32_t size = 0;
  uint32_t value = 0;
  bool mmio = false;
  bool cache_used = false;
  bool cache_hit = false;
};

struct CacheEvent {
  std::string cache;
  std::string op;
  uint32_t paddr = 0;
  uint32_t size = 0;
  bool hit = false;
  int latency = 0;
};

struct CycleRecord {
  uint64_t cycle = 0;
  uint32_t pc = 0;
  uint64_t instrs_retired = 0;
  bool stalled = false;

  StageState if_stage;
  StageState id_stage;
  StageState ex_stage;
  StageState mem_stage;
  StageState wb_stage;

  std::vector<RegWrite> regs_changed;
  std::vector<MemAccess> mem_accesses;
  std::vector<CacheEvent> cache_events;

  std::string exception;
  int tohost = -1;

  uint64_t icache_accesses = 0;
  uint64_t icache_hits = 0;
  uint64_t dcache_accesses = 0;
  uint64_t dcache_hits = 0;
};

class TraceWriter {
public:
  static std::unique_ptr<TraceWriter> Create(const std::string& target, std::string* error = nullptr);

  TraceWriter(const TraceWriter&) = delete;
  TraceWriter& operator=(const TraceWriter&) = delete;

  void WriteCycle(const CycleRecord& record);

private:
  TraceWriter() = default;

  std::ostream* out_ = nullptr;
  std::unique_ptr<std::ostream> owned_out_;
};

}  // namespace trace
