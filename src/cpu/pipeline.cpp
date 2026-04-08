#include "cpu/pipeline.h"
#include "periph/timer_mmio.h"
#include "periph/uart_mmio.h"
#include "elf/elf_loader.h"
#include "periph/tohost_mmio.h"
#include <sstream>
#include <thread>
#include <atomic>
#include <poll.h>
#include <unistd.h>
#include <iostream>

namespace cpu {

Pipeline::Pipeline(const std::vector<uint32_t>& program, size_t mem_size) : program_(program), mem_(mem_size) {
  reset();
  // instantiate caches
  // default miss latency set to 0 for tests; can be tuned later
  // use a configurable 4-way set-associative cache by default
  CacheConfig ccfg;
  ccfg.cache_size = 16 * 1024;
  ccfg.line_size = 64;
  ccfg.associativity = 4;
  ccfg.miss_latency = 0;
  icache_ = new SimpleCache(&mem_, ccfg);
  dcache_ = new SimpleCache(&mem_, ccfg);
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
  // instantiate MMU (uses mem_ and csr_)
  mmu_ = new mmu::MMU(&mem_, &csr_);
}

void Pipeline::reset() {
  pc_ = 0;
  ifid_ = IFIDReg();
  idex_ = IDEXReg();
  exmem_ = EXMEMReg();
  memwb_ = MEMWBReg();
  regs_ = RegisterFile();
  stall_remaining_ = 0;
  stall_kind_ = StallKind::None;
  last_stall_ = false;
}

bool Pipeline::step() {
  constexpr int kCacheMissStallCycles = 10;
  last_stall_ = false;
  // each call to step represents one cycle
  ++cycles_;

  // During a cache miss stall, only advance cycle counter and countdown.
  if (stall_remaining_ > 0) {
    --stall_remaining_;
    return true;
  }

  // Once stall countdown completes, materialize the pending cache result.
  if (stall_kind_ == StallKind::If) {
    ifid_.inst = isa::decode(pending_if_word_);
    ifid_.pc = pending_if_pc_;
    ifid_.valid = true;
    stall_kind_ = StallKind::None;
    return true;
  }
  if (stall_kind_ == StallKind::MemLoad) {
    pending_memwb_.mem_data = pending_mem_value_;
    memwb_ = pending_memwb_;
    stall_kind_ = StallKind::None;
    return true;
  }

  // Snapshot previous stage registers
  IFIDReg prev_ifid = ifid_;
  IDEXReg prev_idex = idex_;
  EXMEMReg prev_exmem = exmem_;
  MEMWBReg prev_memwb = memwb_;

  // count retired instruction if one was in MEM/WB at start of cycle
  if (prev_memwb.valid) ++instrs_;

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
    uint32_t cause_raw = (1u << 31) | 7u;
    uint32_t vec = csr_.handle_trap(cause_raw, 0u, pc_, true);
    pc_ = vec;
    // flush pipeline
    ifid_.valid = false;
    idex_.valid = false;
    exmem_.valid = false;
    memwb_.valid = false;
    csr_.set_mip_timer(false);
    return false;
  } else if (csr_.pending_uart_interrupt()) {
    uint32_t cause_raw = (1u << 31) | 3u;
    uint32_t vec = csr_.handle_trap(cause_raw, 0u, pc_, true);
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
      uint32_t phys = prev_exmem.alu_result;
      uint32_t cause = 0;
      if (mmu_ && !mmu_->translate(prev_exmem.alu_result, mmu::AccessType::Load, phys, cause)) {
        uint32_t vec = csr_.handle_trap(cause, prev_exmem.alu_result, prev_exmem.pc, false);
        pc_ = vec;
        ifid_.valid = false; idex_.valid = false; exmem_.valid = false; memwb_.valid = false;
        return false;
      }
      bool is_mmio = mem_.is_mapped_device_range(phys, 4);
      bool aligned = ((phys & 0x3u) == 0);
      bool use_dcache = dcache_ && !is_mmio && aligned;
      if (use_dcache) {
        auto res = dcache_->load32(phys);
        if (res.second == 0) {
          memwb_.mem_data = res.first;
        } else {
          stall_remaining_ = kCacheMissStallCycles;
          stall_kind_ = StallKind::MemLoad;
          pending_memwb_ = memwb_;
          pending_mem_value_ = res.first;
          // Keep younger stages frozen, but clear current EX/MEM to avoid replaying this load.
          exmem_.valid = false;
          return true;
        }
      } else {
        if (dcache_ && !is_mmio && !aligned) {
          // Direct misaligned read must observe prior dirty cache data.
          dcache_->flush_all();
        }
        memwb_.mem_data = mem_.load32(phys);
        if (!dcache_ && uncached_latency_ > 0 && !mem_.is_mapped_device_range(phys, 4)) {
          cycles_ += static_cast<uint64_t>(uncached_latency_);
        }
      }
    } else if (mname == "LB" || mname == "LBU") {
      uint32_t phys = prev_exmem.alu_result;
      uint32_t cause = 0;
      if (mmu_ && !mmu_->translate(prev_exmem.alu_result, mmu::AccessType::Load, phys, cause)) {
        uint32_t vec = csr_.handle_trap(cause, prev_exmem.alu_result, prev_exmem.pc, false);
        pc_ = vec;
        ifid_.valid = false; idex_.valid = false; exmem_.valid = false; memwb_.valid = false;
        return false;
      }
      if (dcache_ && !mem_.is_mapped_device_range(phys, 1)) {
        auto res = dcache_->load8(phys);
        if (res.second == 0) {
          uint32_t byte = res.first & 0xffu;
          if (mname == "LB") {
            int32_t sval = static_cast<int32_t>(static_cast<int8_t>(byte & 0xff));
            memwb_.mem_data = static_cast<uint32_t>(sval);
          } else {
            memwb_.mem_data = byte;
          }
        } else {
          stall_remaining_ = kCacheMissStallCycles;
          stall_kind_ = StallKind::MemLoad;
          pending_memwb_ = memwb_;
          uint32_t byte = res.first & 0xffu;
          if (mname == "LB") {
            int32_t sval = static_cast<int32_t>(static_cast<int8_t>(byte & 0xff));
            pending_mem_value_ = static_cast<uint32_t>(sval);
          } else {
            pending_mem_value_ = byte;
          }
          // Keep younger stages frozen, but clear current EX/MEM to avoid replaying this load.
          exmem_.valid = false;
          return true;
        }
      } else {
        uint32_t byte = mem_.load8(phys);
        if (mname == "LB") {
          int32_t sval = static_cast<int32_t>(static_cast<int8_t>(byte & 0xff));
          memwb_.mem_data = static_cast<uint32_t>(sval);
        } else {
          memwb_.mem_data = byte;
        }
        if (!dcache_ && uncached_latency_ > 0 && !mem_.is_mapped_device_range(phys, 1)) {
          cycles_ += static_cast<uint64_t>(uncached_latency_);
        }
      }
    } else if (mname == "LH" || mname == "LHU") {
      uint32_t phys = prev_exmem.alu_result;
      uint32_t cause = 0;
      if (mmu_ && !mmu_->translate(prev_exmem.alu_result, mmu::AccessType::Load, phys, cause)) {
        uint32_t vec = csr_.handle_trap(cause, prev_exmem.alu_result, prev_exmem.pc, false);
        pc_ = vec;
        ifid_.valid = false; idex_.valid = false; exmem_.valid = false; memwb_.valid = false;
        return false;
      }
      bool is_mmio = mem_.is_mapped_device_range(phys, 2);
      bool aligned = ((phys & 0x1u) == 0);
      bool use_dcache = dcache_ && !is_mmio && aligned;
      if (use_dcache) {
        auto res = dcache_->load16(phys);
        if (res.second == 0) {
          uint32_t half = res.first & 0xffffu;
          if (mname == "LH") {
            int32_t sval = static_cast<int32_t>(static_cast<int16_t>(half & 0xffff));
            memwb_.mem_data = static_cast<uint32_t>(sval);
          } else {
            memwb_.mem_data = half;
          }
        } else {
          stall_remaining_ = kCacheMissStallCycles;
          stall_kind_ = StallKind::MemLoad;
          pending_memwb_ = memwb_;
          uint32_t half = res.first & 0xffffu;
          if (mname == "LH") {
            int32_t sval = static_cast<int32_t>(static_cast<int16_t>(half & 0xffff));
            pending_mem_value_ = static_cast<uint32_t>(sval);
          } else {
            pending_mem_value_ = half;
          }
          // Keep younger stages frozen, but clear current EX/MEM to avoid replaying this load.
          exmem_.valid = false;
          return true;
        }
      } else {
        if (dcache_ && !is_mmio && !aligned) {
          // Direct misaligned read must observe prior dirty cache data.
          dcache_->flush_all();
        }
        uint32_t half = mem_.load16(phys);
        if (mname == "LH") {
          int32_t sval = static_cast<int32_t>(static_cast<int16_t>(half & 0xffff));
          memwb_.mem_data = static_cast<uint32_t>(sval);
        } else {
          memwb_.mem_data = half;
        }
        if (!dcache_ && uncached_latency_ > 0 && !mem_.is_mapped_device_range(phys, 2)) {
          cycles_ += static_cast<uint64_t>(uncached_latency_);
        }
      }
    } else if (mname == "SW") {
      uint32_t phys = prev_exmem.alu_result;
      uint32_t cause = 0;
      if (mmu_ && !mmu_->translate(prev_exmem.alu_result, mmu::AccessType::Store, phys, cause)) {
        uint32_t vec = csr_.handle_trap(cause, prev_exmem.alu_result, prev_exmem.pc, false);
        pc_ = vec;
        ifid_.valid = false; idex_.valid = false; exmem_.valid = false; memwb_.valid = false;
        return false;
      }
      bool is_mmio = mem_.is_mapped_device_range(phys, 4);
      bool aligned = ((phys & 0x3u) == 0);
      bool use_dcache = dcache_ && !is_mmio && aligned;
      if (use_dcache) dcache_->store32(phys, prev_exmem.rs2_val);
      else {
        mem_.store32(phys, prev_exmem.rs2_val);
        if (dcache_ && !is_mmio && !aligned) {
          // Keep cache coherent after direct misaligned write.
          dcache_->flush_all();
          dcache_->invalidate_all();
        }
      }
      if (!dcache_ && uncached_latency_ > 0 && !is_mmio) {
        cycles_ += static_cast<uint64_t>(uncached_latency_);
      }
    } else if (mname == "SB") {
      uint32_t phys = prev_exmem.alu_result;
      uint32_t cause = 0;
      if (mmu_ && !mmu_->translate(prev_exmem.alu_result, mmu::AccessType::Store, phys, cause)) {
        uint32_t vec = csr_.handle_trap(cause, prev_exmem.alu_result, prev_exmem.pc, false);
        pc_ = vec;
        ifid_.valid = false; idex_.valid = false; exmem_.valid = false; memwb_.valid = false;
        return false;
      }
      bool is_mmio = mem_.is_mapped_device_range(phys, 1);
      if (dcache_ && !is_mmio) dcache_->store8(phys, static_cast<uint8_t>(prev_exmem.rs2_val & 0xffu));
      else mem_.store8(phys, static_cast<uint8_t>(prev_exmem.rs2_val & 0xffu));
      if (!dcache_ && uncached_latency_ > 0 && !is_mmio) {
        cycles_ += static_cast<uint64_t>(uncached_latency_);
      }
    } else if (mname == "SH") {
      uint32_t phys = prev_exmem.alu_result;
      uint32_t cause = 0;
      if (mmu_ && !mmu_->translate(prev_exmem.alu_result, mmu::AccessType::Store, phys, cause)) {
        uint32_t vec = csr_.handle_trap(cause, prev_exmem.alu_result, prev_exmem.pc, false);
        pc_ = vec;
        ifid_.valid = false; idex_.valid = false; exmem_.valid = false; memwb_.valid = false;
        return false;
      }
      bool is_mmio = mem_.is_mapped_device_range(phys, 2);
      bool aligned = ((phys & 0x1u) == 0);
      bool use_dcache = dcache_ && !is_mmio && aligned;
      if (use_dcache) dcache_->store16(phys, static_cast<uint16_t>(prev_exmem.rs2_val & 0xffffu));
      else {
        mem_.store16(phys, static_cast<uint16_t>(prev_exmem.rs2_val & 0xffffu));
        if (dcache_ && !is_mmio && !aligned) {
          // Keep cache coherent after direct misaligned write.
          dcache_->flush_all();
          dcache_->invalidate_all();
        }
      }
      if (!dcache_ && uncached_latency_ > 0 && !is_mmio) {
        cycles_ += static_cast<uint64_t>(uncached_latency_);
      }
    }
  }

  // ------------------ EX stage ------------------
  exmem_ = EXMEMReg();
  bool branch_taken = false;
  uint32_t branch_target = 0;
  if (prev_idex.valid) {
    // handle ECALL -> synchronous trap
    if (prev_idex.inst.name == "ECALL") {
      int priv = csr_.get_privilege();
      uint32_t cause_num = (priv == 0) ? 8u : (priv == 1 ? 9u : 11u);
      uint32_t vec = csr_.handle_trap(cause_num, 0u, prev_idex.pc, false);
      pc_ = vec;
      // flush pipeline
      ifid_.valid = false;
      idex_.valid = false;
      exmem_.valid = false;
      memwb_.valid = false;
      return false;
    }

    // handle MRET -> return from trap
    if (prev_idex.inst.name == "MRET") {
      std::cerr << "Pipeline: executing MRET at pc=0x" << std::hex << prev_idex.pc << std::dec << "\n";
      csr_.mret_restore();
      pc_ = csr_.read_mepc();
      std::cerr << "Pipeline: MRET returned to pc=0x" << std::hex << pc_ << std::dec << " privilege=" << csr_.get_privilege() << "\n";
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
    else if (name == "LUI") alu_res = static_cast<uint32_t>(prev_idex.inst.imm);
    else if (name == "AUIPC") alu_res = prev_idex.pc + static_cast<uint32_t>(prev_idex.inst.imm);
    else if (name == "SLLI") alu_res = a << (static_cast<uint32_t>(prev_idex.inst.imm) & 0x1f);
    else if (name == "SLTI") alu_res = (static_cast<int32_t>(a) < static_cast<int32_t>(prev_idex.inst.imm)) ? 1u : 0u;
    else if (name == "SLTIU") alu_res = (a < static_cast<uint32_t>(prev_idex.inst.imm)) ? 1u : 0u;
    else if (name == "XORI") alu_res = a ^ static_cast<uint32_t>(prev_idex.inst.imm);
    else if (name == "SRLI") alu_res = a >> (static_cast<uint32_t>(prev_idex.inst.imm) & 0x1f);
    else if (name == "SRAI") alu_res = static_cast<uint32_t>(static_cast<int32_t>(a) >> (static_cast<uint32_t>(prev_idex.inst.imm) & 0x1f));
    else if (name == "ORI") alu_res = a | static_cast<uint32_t>(prev_idex.inst.imm);
    else if (name == "ANDI") alu_res = a & static_cast<uint32_t>(prev_idex.inst.imm);
    else if (name == "SLL") alu_res = a << (b & 0x1f);
    else if (name == "SRL") alu_res = a >> (b & 0x1f);
    else if (name == "SRA") alu_res = static_cast<uint32_t>(static_cast<int32_t>(a) >> (b & 0x1f));
    else if (name == "OR") alu_res = a | b;
    else if (name == "AND") alu_res = a & b;
    else if (name == "XOR") alu_res = a ^ b;
    else if (name == "SLT") alu_res = (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? 1u : 0u;
    else if (name == "SLTU") alu_res = (a < b) ? 1u : 0u;
    else if (name == "LW" || name == "SW" || name == "LB" || name == "LBU" || name == "LH" || name == "LHU" || name == "SB" || name == "SH") {
      alu_res = a + static_cast<uint32_t>(prev_idex.inst.imm);
      if (verbose_) std::cerr << "EX: " << name << " pc=0x" << std::hex << prev_idex.pc << std::dec << " rs1_val=0x" << std::hex << a << " rs2_val=0x" << b << " imm=0x" << std::hex << prev_idex.inst.imm << " -> alu_res=0x" << alu_res << std::dec << "\n";
    }
    else if (name == "CSRRW" || name == "CSRRS" || name == "CSRRC" || name == "CSRRWI" || name == "CSRRSI" || name == "CSRRCI") {
      uint32_t csr_addr = prev_idex.inst.csr;
      try {
        uint32_t old = csr_.read(csr_addr);
        uint32_t newv = old;
        if (name == "CSRRW") newv = a;
        else if (name == "CSRRWI") newv = prev_idex.inst.rs1; // zimm in rs1 field
        else if (name == "CSRRS") newv = old | a;
        else if (name == "CSRRSI") newv = old | prev_idex.inst.rs1;
        else if (name == "CSRRC") newv = old & ~a;
        else if (name == "CSRRCI") newv = old & ~prev_idex.inst.rs1;
        csr_.write(csr_addr, newv);
        alu_res = old;
        // if SATP changed, flush TLB and perform conservative cache maintenance
        if (csr_addr == 0x180) {
          if (mmu_) mmu_->flush_tlb();
          if (icache_) icache_->invalidate_all();
          if (dcache_) dcache_->flush_all();
        }
      } catch (const CSR::CSRException &ex) {
        uint32_t vec = csr_.handle_trap(ex.cause(), 0u, prev_idex.pc, false);
        pc_ = vec;
        // flush pipeline
        ifid_.valid = false;
        idex_.valid = false;
        exmem_.valid = false;
        memwb_.valid = false;
        return false;
      }
    }
    else if (name == "SFENCE.VMA") {
      // SFENCE.VMA rs1=virtual-address, rs2=asid
      int priv = csr_.get_privilege();
      if (priv == 0) {
        // illegal in U-mode
        uint32_t vec = csr_.handle_trap(2u, 0u, prev_idex.pc, false);
        pc_ = vec;
        ifid_.valid = false; idex_.valid = false; exmem_.valid = false; memwb_.valid = false;
        return false;
      }
      uint32_t vaddr = a; // rs1_val
      uint32_t asid = b;  // rs2_val
      if (mmu_) mmu_->sfence_vma(vaddr, asid);
    }
    else if (name == "FENCE.I") {
      // Make prior stores globally visible and force refetch of subsequent instructions.
      if (dcache_) dcache_->flush_all();
      if (icache_) icache_->invalidate_all();
    }
    else if (name == "FENCE") {
      // Treated as an ordering no-op in this in-order simulator.
    }
    else if (name == "BEQ") {
      if (a == b) { branch_taken = true; branch_target = prev_idex.pc + prev_idex.inst.imm; }
    } else if (name == "BNE") {
      if (a != b) { branch_taken = true; branch_target = prev_idex.pc + prev_idex.inst.imm; }
    } else if (name == "BLT") {
      if (static_cast<int32_t>(a) < static_cast<int32_t>(b)) { branch_taken = true; branch_target = prev_idex.pc + prev_idex.inst.imm; }
    } else if (name == "BGE") {
      if (static_cast<int32_t>(a) >= static_cast<int32_t>(b)) { branch_taken = true; branch_target = prev_idex.pc + prev_idex.inst.imm; }
    } else if (name == "BLTU") {
      if (a < b) { branch_taken = true; branch_target = prev_idex.pc + prev_idex.inst.imm; }
    } else if (name == "BGEU") {
      if (a >= b) { branch_taken = true; branch_target = prev_idex.pc + prev_idex.inst.imm; }
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
    exmem_.write_back = (name == "ADD" || name == "SUB" || name == "ADDI" || name == "SLLI" || name == "SLL" || name == "SRL" || name == "SRA" || name == "OR" || name == "AND" || name == "XOR" || name == "SLT" || name == "SLTU" || name == "LW" || name == "LB" || name == "LH" || name == "LBU" || name == "LHU" || name == "JAL" || name == "JALR" || name == "LUI" || name == "AUIPC" || name == "SLTI" || name == "SLTIU" || name == "XORI" || name == "SRLI" || name == "SRAI" || name == "ORI" || name == "ANDI" || name == "CSRRW" || name == "CSRRS" || name == "CSRRC" || name == "CSRRWI" || name == "CSRRSI" || name == "CSRRCI");

    if (branch_taken) {
      pc_ = branch_target;
      // flush IF/ID (will be handled by not fetching below)
      ifid_.valid = false;
      // squash the already-fetched fall-through instruction in this cycle
      prev_ifid.valid = false;
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
    // Allow fetching from any mapped memory region (not limited to initial program_ vector)
    if (pc_ < static_cast<uint32_t>(mem_.size())) {
      // translate virtual PC to physical
      uint32_t phys_pc = pc_;
      uint32_t cause_out = 0;
      if (mmu_ && !mmu_->translate(pc_, mmu::AccessType::Fetch, phys_pc, cause_out)) {
        uint32_t vec = csr_.handle_trap(cause_out, pc_, pc_, false);
        pc_ = vec;
        ifid_.valid = false;
        idex_.valid = false;
        exmem_.valid = false;
        memwb_.valid = false;
        return false;
      }
      // use icache for instruction fetch (physical address) if configured
      if (icache_) {
        auto res = icache_->load32(phys_pc);
        if (res.second == 0) {
          uint32_t word = res.first;
          // debug: print first few fetched instructions
          static int __dbg_fetch = 0;
          if (__dbg_fetch < 40) {
            auto d = isa::decode(word);
            std::cerr << "FETCH pc=0x" << std::hex << pc_ << " instr=" << d.name << std::dec << "\n";
            ++__dbg_fetch;
          }
          ifid_.inst = isa::decode(word);
          ifid_.pc = pc_;
          ifid_.valid = true;
          pc_ += 4;
        } else {
          // miss: stall pipeline for fixed 10 cycles then deliver
          stall_remaining_ = kCacheMissStallCycles;
          stall_kind_ = StallKind::If;
          pending_if_word_ = res.first;
          pending_if_pc_ = pc_;
          // This fetch is already accepted; advance PC now and clear stale IF/ID.
          pc_ += 4;
          ifid_.valid = false;
          return true;
        }
      } else {
        uint32_t word = mem_.load32(phys_pc);
        if (uncached_latency_ > 0 && !mem_.is_mapped_device_range(phys_pc, 4)) {
          cycles_ += static_cast<uint64_t>(uncached_latency_);
        }
        ifid_.inst = isa::decode(word);
        ifid_.pc = pc_;
        ifid_.valid = true;
        pc_ += 4;
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
  if (icache_) { delete icache_; icache_ = nullptr; }
  if (dcache_) { delete dcache_; dcache_ = nullptr; }
  if (mmu_) { delete mmu_; mmu_ = nullptr; }
}

void Pipeline::configure_cache(bool enable, const CacheConfig& cfg) {
  if (icache_) { delete icache_; icache_ = nullptr; }
  if (dcache_) { delete dcache_; dcache_ = nullptr; }
  if (enable) {
    icache_ = new SimpleCache(&mem_, cfg);
    dcache_ = new SimpleCache(&mem_, cfg);
  } else {
    icache_ = nullptr;
    dcache_ = nullptr;
  }
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
  // if ELF provided a `tohost` symbol, map semihosting device there
  if (info.tohost != 0) {
    if (verbose_) std::cerr << "load_elf: detected tohost at 0x" << std::hex << info.tohost << std::dec << ", mapping ToHostMMIO\n";
    mem_.map_device(info.tohost, 8, new periph::ToHostMMIO(true));
  }

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

  // ensure backing memory includes stack_top; allocate an extra guard page
  uint8_t z = 0;
  mem_.store_bytes(stack_top + page - 1, &z, 1);

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

bool Pipeline::mmu_map_4k(uint32_t vaddr, uint32_t paddr, uint32_t pte_flags) {
  // require 4k alignment
  const uint32_t page = 0x1000u;
  if ((vaddr & (page - 1)) || (paddr & (page - 1))) return false;
  // ensure root page table exists
  uint32_t satp = csr_.read(0x180);
  uint32_t root_ppn = satp & 0x003fffffu;
  uint32_t root_base = root_ppn << 12u;
  // helper: allocate a page-table page in a high offset area to avoid colliding
  // with physical pages tests may pick (they often use mem.size()+0x1000).
  auto alloc_page_avoid = [&](uint32_t avoid_addr) {
    const uint32_t base_offset = 8u * page; // place PT pages well beyond current heap
    uint32_t addr = static_cast<uint32_t>(mem_.size()) + base_offset;
    if (addr == avoid_addr) addr += page;
    std::vector<uint8_t> z(page, 0);
    mem_.store_bytes(addr, z.data(), z.size());
    return addr;
  };

  if (root_ppn == 0) {
    uint32_t rb = alloc_page_avoid(paddr);
    uint32_t new_ppn = rb >> 12u;
    uint32_t new_satp = (1u << 31) | (new_ppn & 0x003fffffu);
    csr_.write(0x180, new_satp);
    root_base = rb;
  }
  uint32_t vpn1 = (vaddr >> 22) & 0x3ffu;
  uint32_t vpn0 = (vaddr >> 12) & 0x3ffu;
  uint32_t pte1_addr = root_base + vpn1 * 4u;
  uint32_t pte1 = 0;
  try { pte1 = mem_.load32(pte1_addr); } catch (...) { pte1 = 0; }
  uint32_t level0_base = 0;
  if (pte1 == 0) {
    uint32_t lvl0 = alloc_page_avoid(paddr);
    uint32_t ppn0 = lvl0 >> 12u;
    uint32_t new_pte1 = (ppn0 << 10u) | 1u; // valid pointer
    std::cerr << "mmu_map_4k: alloc level0 page at 0x" << std::hex << lvl0 << std::dec << " for vpn1=" << vpn1 << "\n";
    std::cerr << "mmu_map_4k: write pte1 @0x" << std::hex << pte1_addr << " = 0x" << new_pte1 << std::dec << "\n";
    mem_.store32(pte1_addr, new_pte1);
    level0_base = lvl0;
  } else {
    uint32_t next_ppn = pte1 >> 10u;
    level0_base = next_ppn << 12u;
  }
  uint32_t pte0_addr = level0_base + vpn0 * 4u;
  uint32_t pte0 = ((paddr >> 12u) << 10u) | (pte_flags & 0xffu);
  std::cerr << "mmu_map_4k: write pte0 @0x" << std::hex << pte0_addr << " = 0x" << pte0 << std::dec << " (vaddr=0x" << std::hex << vaddr << ")\n";
  mem_.store32(pte0_addr, pte0);
  if (mmu_) mmu_->set_enabled(true);
  return true;
}

void Pipeline::set_verbose(bool v) {
  verbose_ = v;
  if (mmu_) mmu_->set_verbose(v);
}

void Pipeline::sfence_vma(uint32_t vaddr, uint32_t asid) {
  if (mmu_) mmu_->sfence_vma(vaddr, asid);
  // conservative cache maintenance: invalidate instruction cache and flush data cache
  if (icache_) icache_->invalidate_all();
  if (dcache_) dcache_->flush_all();
}

unsigned Pipeline::tlb_count() const {
  return mmu_ ? mmu_->tlb_count() : 0u;
}

} // namespace cpu
