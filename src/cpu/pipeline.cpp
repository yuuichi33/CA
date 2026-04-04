#include "cpu/pipeline.h"
#include "periph/timer_mmio.h"
#include "periph/uart_mmio.h"
#include "elf/elf_loader.h"
#include <sstream>
#include <thread>
#include <atomic>
#include <poll.h>
#include <unistd.h>

namespace cpu {

Pipeline::Pipeline(const std::vector<uint32_t>& program, size_t mem_size) : program_(program), mem_(mem_size) {
  reset();
  // instantiate caches
  // default miss latency set to 0 for tests; can be tuned later
  icache_ = new SimpleCache(&mem_, 16 * 1024, 64, 0);
  dcache_ = new SimpleCache(&mem_, 16 * 1024, 64, 0);
  // map timer MMIO region (keep pointer for possible future use)
  constexpr uint32_t TIMER_MMIO_BASE = 0x10000000u;
  timer_mmio_ = new periph::TimerMMIO(&timer_);
  mem_.map_device(TIMER_MMIO_BASE, 0x20, timer_mmio_);
  // map UART MMIO region (keep pointer so tests can inject RX)
  constexpr uint32_t UART_MMIO_BASE = 0x10001000u;
  uart_mmio_ = new periph::UARTMMIO(&csr_);
  mem_.map_device(UART_MMIO_BASE, 0x20, uart_mmio_);
  // copy program into physical memory at address 0
  for (size_t i = 0; i < program_.size(); ++i) {
    mem_.store32(static_cast<uint32_t>(i * 4), program_[i]);
  }
}

void Pipeline::reset() {
  pc_ = 0;
  ifid_ = IFIDReg();
  idex_ = IDEXReg();
  exmem_ = EXMEMReg();
  memwb_ = MEMWBReg();
  regs_ = RegisterFile();
  last_stall_ = false;
}

bool Pipeline::step() {
  last_stall_ = false;

  // if currently stalling for cache miss, countdown
  if (stall_counter_ > 0) {
    --stall_counter_;
    if (stall_counter_ == 0) {
      if (stall_kind_ == StallKind::If) {
        // commit pending IF fetch
        ifid_.inst = isa::decode(pending_if_word_);
        ifid_.pc = pending_if_pc_;
        ifid_.valid = true;
        stall_kind_ = StallKind::None;
      } else if (stall_kind_ == StallKind::MemLoad) {
        // commit pending memory load into memwb_
        pending_memwb_.mem_data = pending_mem_value_;
        memwb_ = pending_memwb_;
        stall_kind_ = StallKind::None;
      }
    }
    return true;
  }

  // Snapshot previous stage registers
  IFIDReg prev_ifid = ifid_;
  IDEXReg prev_idex = idex_;
  EXMEMReg prev_exmem = exmem_;
  MEMWBReg prev_memwb = memwb_;

  // ------------------ WB stage ------------------
  if (prev_memwb.valid && prev_memwb.write_back && prev_memwb.rd != 0) {
    const std::string &wbname = prev_memwb.inst.name;
    if (wbname == "LW" || wbname == "LB" || wbname == "LH" || wbname == "LBU" || wbname == "LHU") {
      regs_.write(prev_memwb.rd, prev_memwb.mem_data);
    } else {
      regs_.write(prev_memwb.rd, prev_memwb.alu_result);
    }
  }

  // timer tick and interrupt injection
  if (timer_.tick()) {
    csr_.set_mip_timer(true);
  }

  // handle pending interrupts: timer first, then UART
  if (csr_.pending_timer_interrupt()) {
    // set mepc to current pc_
    csr_.write_mepc(pc_);
    // set mcause with interrupt bit + timer IRQ number (7)
    csr_.write_mcause((1u << 31) | 7u);
    uint32_t vec = csr_.read_mtvec();
    pc_ = vec;
    // flush pipeline
    ifid_.valid = false;
    idex_.valid = false;
    exmem_.valid = false;
    memwb_.valid = false;
    csr_.set_mip_timer(false);
    return false;
  } else if (csr_.pending_uart_interrupt()) {
    csr_.write_mepc(pc_);
    csr_.write_mcause((1u << 31) | 3u);
    uint32_t vec = csr_.read_mtvec();
    pc_ = vec;
    ifid_.valid = false;
    idex_.valid = false;
    exmem_.valid = false;
    memwb_.valid = false;
    csr_.set_mip_uart(false);
    return false;
  }

  // ------------------ MEM stage ------------------
  memwb_ = MEMWBReg();
  if (prev_exmem.valid) {
    memwb_.inst = prev_exmem.inst;
    memwb_.pc = prev_exmem.pc;
    memwb_.valid = true;
    memwb_.rd = prev_exmem.rd;
    memwb_.write_back = prev_exmem.write_back;
    memwb_.alu_result = prev_exmem.alu_result;

    const std::string &mname = prev_exmem.inst.name;
    if (mname == "LW") {
      auto res = dcache_->load32(prev_exmem.alu_result);
      if (res.second == 0) {
        memwb_.mem_data = res.first;
      } else {
        stall_counter_ = res.second;
        stall_kind_ = StallKind::MemLoad;
        pending_memwb_ = memwb_;
        pending_mem_value_ = res.first;
        return true;
      }
    } else if (mname == "LB" || mname == "LBU") {
      auto res = dcache_->load8(prev_exmem.alu_result);
      if (res.second == 0) {
        uint32_t byte = res.first & 0xffu;
        if (mname == "LB") {
          int32_t sval = static_cast<int32_t>(static_cast<int8_t>(byte & 0xff));
          memwb_.mem_data = static_cast<uint32_t>(sval);
        } else {
          memwb_.mem_data = byte;
        }
      } else {
        stall_counter_ = res.second;
        stall_kind_ = StallKind::MemLoad;
        pending_memwb_ = memwb_;
        pending_mem_value_ = res.first & 0xffu;
        return true;
      }
    } else if (mname == "LH" || mname == "LHU") {
      auto res = dcache_->load16(prev_exmem.alu_result);
      if (res.second == 0) {
        uint32_t half = res.first & 0xffffu;
        if (mname == "LH") {
          int32_t sval = static_cast<int32_t>(static_cast<int16_t>(half & 0xffff));
          memwb_.mem_data = static_cast<uint32_t>(sval);
        } else {
          memwb_.mem_data = half;
        }
      } else {
        stall_counter_ = res.second;
        stall_kind_ = StallKind::MemLoad;
        pending_memwb_ = memwb_;
        pending_mem_value_ = res.first & 0xffffu;
        return true;
      }
    } else if (mname == "SW") {
      dcache_->store32(prev_exmem.alu_result, prev_exmem.rs2_val);
    } else if (mname == "SB") {
      dcache_->store8(prev_exmem.alu_result, static_cast<uint8_t>(prev_exmem.rs2_val & 0xffu));
    } else if (mname == "SH") {
      dcache_->store16(prev_exmem.alu_result, static_cast<uint16_t>(prev_exmem.rs2_val & 0xffffu));
    }
  }

  // ------------------ EX stage ------------------
  exmem_ = EXMEMReg();
  bool branch_taken = false;
  uint32_t branch_target = 0;
  if (prev_idex.valid) {
    // handle ECALL -> synchronous trap
    if (prev_idex.inst.name == "ECALL") {
      csr_.write_mepc(prev_idex.pc);
      csr_.write_mcause(11u); // environment call from M-mode
      pc_ = csr_.read_mtvec();
      // flush pipeline
      ifid_.valid = false;
      idex_.valid = false;
      exmem_.valid = false;
      memwb_.valid = false;
      return false;
    }

    // handle MRET -> return from trap
    if (prev_idex.inst.name == "MRET") {
      pc_ = csr_.read_mepc();
      // clear pending flags
      csr_.set_mip_timer(false);
      csr_.set_mip_uart(false);
      ifid_.valid = false;
      idex_.valid = false;
      exmem_.valid = false;
      memwb_.valid = false;
      return false;
    }
    // operand fetch with forwarding: prioritize EX/MEM, then MEM/WB
    uint32_t a = prev_idex.rs1_val;
    uint32_t b = prev_idex.rs2_val;

    // forward for rs1
    if (prev_exmem.valid && prev_exmem.rd != 0 && prev_exmem.write_back && prev_exmem.rd == prev_idex.inst.rs1) {
      const std::string &exname = prev_exmem.inst.name;
      if (!(exname == "LW" || exname == "LB" || exname == "LH" || exname == "LBU" || exname == "LHU")) {
        a = prev_exmem.alu_result;
      }
    } else if (prev_memwb.valid && prev_memwb.rd != 0 && prev_memwb.write_back && prev_memwb.rd == prev_idex.inst.rs1) {
      const std::string &wbname2 = prev_memwb.inst.name;
      if (wbname2 == "LW" || wbname2 == "LB" || wbname2 == "LH" || wbname2 == "LBU" || wbname2 == "LHU") a = prev_memwb.mem_data; else a = prev_memwb.alu_result;
    }

    // forward for rs2
    if (prev_exmem.valid && prev_exmem.rd != 0 && prev_exmem.write_back && prev_exmem.rd == prev_idex.inst.rs2) {
      const std::string &exname2 = prev_exmem.inst.name;
      if (!(exname2 == "LW" || exname2 == "LB" || exname2 == "LH" || exname2 == "LBU" || exname2 == "LHU")) {
        b = prev_exmem.alu_result;
      }
    } else if (prev_memwb.valid && prev_memwb.rd != 0 && prev_memwb.write_back && prev_memwb.rd == prev_idex.inst.rs2) {
      const std::string &wbname3 = prev_memwb.inst.name;
      if (wbname3 == "LW" || wbname3 == "LB" || wbname3 == "LH" || wbname3 == "LBU" || wbname3 == "LHU") b = prev_memwb.mem_data; else b = prev_memwb.alu_result;
    }

    uint32_t alu_res = 0;
    const std::string &name = prev_idex.inst.name;
    if (name == "ADD") alu_res = a + b;
    else if (name == "SUB") alu_res = a - b;
    else if (name == "ADDI") alu_res = a + static_cast<uint32_t>(prev_idex.inst.imm);
    else if (name == "SLLI") alu_res = a << (static_cast<uint32_t>(prev_idex.inst.imm) & 0x1f);
    else if (name == "SLL") alu_res = a << (b & 0x1f);
    else if (name == "SRL") alu_res = a >> (b & 0x1f);
    else if (name == "SRA") alu_res = static_cast<uint32_t>(static_cast<int32_t>(a) >> (b & 0x1f));
    else if (name == "OR") alu_res = a | b;
    else if (name == "AND") alu_res = a & b;
    else if (name == "XOR") alu_res = a ^ b;
    else if (name == "SLT") alu_res = (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? 1u : 0u;
    else if (name == "SLTU") alu_res = (a < b) ? 1u : 0u;
    else if (name == "LW" || name == "SW" || name == "LB" || name == "LBU" || name == "LH" || name == "LHU" || name == "SB" || name == "SH") alu_res = a + static_cast<uint32_t>(prev_idex.inst.imm);
    else if (name == "BEQ") {
      if (a == b) { branch_taken = true; branch_target = prev_idex.pc + prev_idex.inst.imm; }
    } else if (name == "BNE") {
      if (a != b) { branch_taken = true; branch_target = prev_idex.pc + prev_idex.inst.imm; }
    } else if (name == "JAL") {
      alu_res = prev_idex.pc + 4;
      branch_taken = true;
      branch_target = prev_idex.pc + prev_idex.inst.imm;
    } else if (name == "JALR") {
      alu_res = prev_idex.pc + 4;
      branch_taken = true;
      branch_target = (a + static_cast<uint32_t>(prev_idex.inst.imm)) & ~1u;
    }

    exmem_.inst = prev_idex.inst;
    exmem_.pc = prev_idex.pc;
    exmem_.valid = prev_idex.valid;
    exmem_.alu_result = alu_res;
    exmem_.rs2_val = b;
    exmem_.rd = prev_idex.inst.rd;
    // determine if instruction writes back
    exmem_.write_back = (name == "ADD" || name == "SUB" || name == "ADDI" || name == "SLLI" || name == "SLL" || name == "SRL" || name == "SRA" || name == "OR" || name == "AND" || name == "XOR" || name == "SLT" || name == "SLTU" || name == "LW" || name == "LB" || name == "LH" || name == "LBU" || name == "LHU" || name == "JAL" || name == "JALR" || name == "LUI" || name == "AUIPC");

    if (branch_taken) {
      pc_ = branch_target;
      // flush IF/ID (will be handled by not fetching below)
      ifid_.valid = false;
    }
  }

  // ------------------ ID and IF stage ------------------
  // hazard detection: load-use (prev_idex is a load and prev_ifid uses its rd)
  bool hazard = false;
  if (prev_idex.valid && (prev_idex.inst.name == "LW" || prev_idex.inst.name == "LB" || prev_idex.inst.name == "LH" || prev_idex.inst.name == "LBU" || prev_idex.inst.name == "LHU") && prev_ifid.valid) {
    uint32_t rs1 = prev_ifid.inst.rs1;
    uint32_t rs2 = prev_ifid.inst.rs2;
    if ((rs1 != 0 && rs1 == prev_idex.inst.rd) || (rs2 != 0 && rs2 == prev_idex.inst.rd)) {
      hazard = true;
    }
  }

  if (hazard) {
    isa::Decoded nop;
    nop.name = "NOP";
    nop.type = isa::InstType::OTHER;
    idex_.inst = nop;
    idex_.valid = true;
    last_stall_ = true;
    // do not fetch, keep ifid_ unchanged
  } else {
    // move IF/ID -> ID/EX and read regs (note: WB already happened earlier this cycle)
    if (prev_ifid.valid) {
      idex_.inst = prev_ifid.inst;
      idex_.pc = prev_ifid.pc;
      idex_.valid = prev_ifid.valid;
      idex_.rs1_val = regs_.read(prev_ifid.inst.rs1);
      idex_.rs2_val = regs_.read(prev_ifid.inst.rs2);
    } else {
      idex_ = IDEXReg();
    }

    // fetch next instruction into IF/ID unless we flushed due to branch
    if (pc_ / 4 < program_.size()) {
      // use icache for instruction fetch
      auto res = icache_->load32(pc_);
      if (res.second == 0) {
        uint32_t word = res.first;
        ifid_.inst = isa::decode(word);
        ifid_.pc = pc_;
        ifid_.valid = true;
        pc_ += 4;
      } else {
        // miss: stall pipeline for res.second cycles then deliver
        stall_counter_ = res.second;
        stall_kind_ = StallKind::If;
        pending_if_word_ = res.first;
        pending_if_pc_ = pc_;
        return true;
      }
    } else {
      ifid_.valid = false;
    }
  }

  return last_stall_;
}

void Pipeline::inject_uart_rx(uint8_t b) {
  if (uart_mmio_) uart_mmio_->inject_rx(b);
}

void Pipeline::uart_write_ier(uint32_t ier) {
  if (uart_mmio_) {
    // IER is at offset 0x04
    uart_mmio_->store32(0x04, ier);
  }
}

Pipeline::~Pipeline() {
  stop_uart_stdin_bridge();
}

void Pipeline::start_uart_stdin_bridge() {
  if (!uart_mmio_) return;
  if (uart_stdin_running_.load()) return;
  uart_stdin_stop_.store(false);
  uart_stdin_running_.store(true);
  uart_stdin_thread_ = std::thread([this]() {
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    while (!uart_stdin_stop_.load()) {
      int ret = poll(&pfd, 1, 200);
      if (ret > 0 && (pfd.revents & POLLIN)) {
        char buf[128];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0) {
          for (ssize_t i = 0; i < n; ++i) {
            inject_uart_rx(static_cast<uint8_t>(buf[i]));
          }
        } else if (n == 0) {
          // EOF, stop bridge
          break;
        }
      }
    }
    uart_stdin_running_.store(false);
  });
}

void Pipeline::stop_uart_stdin_bridge() {
  if (!uart_stdin_running_.load()) return;
  uart_stdin_stop_.store(true);
  if (uart_stdin_thread_.joinable()) uart_stdin_thread_.join();
  uart_stdin_running_.store(false);
}

bool Pipeline::load_elf(const std::string& path, const std::vector<std::string>& argv_in, const std::vector<std::string>& envp_in) {
  uint32_t entry = 0;
  elf::LoadInfo info;
  if (!elf::load_elf(path, mem_, entry, &info)) return false;
  pc_ = entry;

  // prepare argv/envp
  std::vector<std::string> argv = argv_in;
  std::vector<std::string> envp = envp_in;
  if (argv.empty()) argv.push_back(path.empty() ? std::string("program") : path);

  // stack layout and sizing
  const uint32_t page = 0x1000u;
  const uint32_t reserve = 0x10000u; // reserve 64KB for stack
  const uint32_t guard = page; // guard page at top
  // align stack top up to page boundary to avoid partial pages
  uint32_t stack_top = static_cast<uint32_t>(((mem_.size() + reserve + page - 1) / page) * page);
  uint32_t sp = stack_top - guard;
  auto align_down = [](uint32_t v, uint32_t a) { return v & ~(a - 1); };

  // ensure backing memory includes stack_top
  uint8_t z = 0;
  mem_.store_bytes(stack_top - 1, &z, 1);

  // allocate strings (argv and envp) from high addresses downward
  uint32_t cur = sp;
  std::vector<uint32_t> argv_ptrs(argv.size());
  for (int i = static_cast<int>(argv.size()) - 1; i >= 0; --i) {
    const auto &s = argv[i];
    uint32_t len = static_cast<uint32_t>(s.size());
    if (len + 1 > cur) return false;
    cur -= (len + 1);
    mem_.store_bytes(cur, reinterpret_cast<const uint8_t*>(s.c_str()), len + 1);
    argv_ptrs[i] = cur;
  }

  std::vector<uint32_t> env_ptrs(envp.size());
  for (int i = static_cast<int>(envp.size()) - 1; i >= 0; --i) {
    const auto &s = envp[i];
    uint32_t len = static_cast<uint32_t>(s.size());
    if (len + 1 > cur) return false;
    cur -= (len + 1);
    mem_.store_bytes(cur, reinterpret_cast<const uint8_t*>(s.c_str()), len + 1);
    env_ptrs[i] = cur;
  }

  // align down to 4 for pointer area
  cur = align_down(cur, 4);

  // build auxv
  const uint32_t AT_NULL = 0;
  const uint32_t AT_PHDR = 3;
  const uint32_t AT_PHENT = 4;
  const uint32_t AT_PHNUM = 5;
  const uint32_t AT_PAGESZ = 6;
  const uint32_t AT_BASE = 7;
  const uint32_t AT_ENTRY = 9;

  std::vector<std::pair<uint32_t,uint32_t>> auxv;
  uint32_t phdr_runtime = info.phdr;
  auxv.push_back({AT_PHDR, phdr_runtime});
  auxv.push_back({AT_PHENT, info.phent});
  auxv.push_back({AT_PHNUM, info.phnum});
  auxv.push_back({AT_PAGESZ, 4096});
  auxv.push_back({AT_ENTRY, entry});
  if (info.base != 0) auxv.push_back({AT_BASE, info.base});
  auxv.push_back({AT_NULL, 0});

  // compute size needed for argc/argv/envp/auxv
  uint32_t argc = static_cast<uint32_t>(argv.size());
  uint32_t envc = static_cast<uint32_t>(envp.size());
  uint32_t ptrs_bytes = 4 /*argc*/ + (argc + 1 + envc + 1) * 4 + static_cast<uint32_t>(auxv.size()) * 2 * 4;

  uint32_t new_sp = align_down(cur - ptrs_bytes, 16);

  // ensure backing memory covers stack_top (already done), now write pointer area
  uint32_t addr = new_sp;
  mem_.store32(addr, argc); addr += 4;
  // argv pointers
  for (uint32_t i = 0; i < argc; ++i) { mem_.store32(addr, argv_ptrs[i]); addr += 4; }
  mem_.store32(addr, 0); addr += 4; // argv NULL
  // envp pointers
  for (uint32_t i = 0; i < envc; ++i) { mem_.store32(addr, env_ptrs[i]); addr += 4; }
  mem_.store32(addr, 0); addr += 4; // envp NULL
  // auxv entries
  for (auto &p : auxv) { mem_.store32(addr, p.first); addr += 4; mem_.store32(addr, p.second); addr += 4; }

  // set registers: sp (x2), a0 (x10)=argc, a1 (x11)=argv pointer, a2 (x12)=envp pointer
  regs_.write(2, new_sp);
  regs_.write(10, argc);
  regs_.write(11, new_sp + 4);
  regs_.write(12, new_sp + 4 + (argc + 1) * 4);

  return true;
}

std::string Pipeline::dump_regs() const {
  std::ostringstream oss;
  oss << "PC=" << pc_ << " IF/ID.valid=" << ifid_.valid << " IF/ID.inst=" << ifid_.inst.name
      << " ID/EX.valid=" << idex_.valid << " ID/EX.inst=" << idex_.inst.name;
  return oss.str();
}

} // namespace cpu
