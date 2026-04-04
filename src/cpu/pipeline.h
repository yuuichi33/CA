#pragma once
#include "isa/decoder.h"
#include "cpu/registers.h"
#include "mem/memory.h"
#include "cache/simple_cache.h"
#include "cpu/csr.h"
#include "periph/timer.h"
#include "periph/timer_mmio.h"
#include "periph/uart_mmio.h"
#include <cstdint>
#include <thread>
#include <atomic>
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
  ~Pipeline();
  void reset();
  // step returns true if a stall/bubble was inserted this cycle
  bool step();
  // inject a byte into UART RX FIFO (for tests/host input)
  void inject_uart_rx(uint8_t b);
  // helper to write UART IER register (bit1 = RX IRQ enable)
  void uart_write_ier(uint32_t ier);
  // start/stop stdin -> UART RX bridge (optional, non-blocking)
  void start_uart_stdin_bridge();
  void stop_uart_stdin_bridge();
  // load an ELF file into memory and set PC to its entry
  // optionally provide `argv` and `envp` (defaults to {path} and empty)
  bool load_elf(const std::string& path, const std::vector<std::string>& argv = {}, const std::vector<std::string>& envp = {});
  uint32_t pc() const { return pc_; }
  const IFIDReg& ifid() const { return ifid_; }
  const IDEXReg& idex() const { return idex_; }
  const EXMEMReg& exmem() const { return exmem_; }
  const MEMWBReg& memwb() const { return memwb_; }
  RegisterFile& regs() { return regs_; }
  cpu::CSR& csr() { return csr_; }
  periph::Timer& timer() { return timer_; }
  // expose memory for tests/tools
  mem::Memory& memory() { return mem_; }
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
  // CSRs and timer
  cpu::CSR csr_;
  periph::Timer timer_;
  periph::TimerMMIO* timer_mmio_ = nullptr;
  periph::UARTMMIO* uart_mmio_ = nullptr;
  // stdin bridge thread
  std::thread uart_stdin_thread_;
  std::atomic<bool> uart_stdin_running_{false};
  std::atomic<bool> uart_stdin_stop_{false};
};

} // namespace cpu
