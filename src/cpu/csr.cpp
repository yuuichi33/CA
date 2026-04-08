#include "cpu/csr.h"
#include <iostream>

namespace cpu {

// mstatus MIE bit position (for our simple simulator)
static constexpr uint32_t MSTATUS_MIE = (1u << 3);
// simple position for SIE when composing mstatus for reads
static constexpr uint32_t MSTATUS_SIE = (1u << 1);

CSR::CSR()
  : mstatus_(0), mepc_(0), sepc_(0), scause_(0), stval_(0), mcause_(0), mtvec_(0), stvec_(0), satp_(0), medeleg_(0), mideleg_(0), privilege_(3), mie_timer_(false), mip_timer_(false), mie_uart_(false), mip_uart_(false), sum_(false), mpp_(3), mpie_(false), spie_(false), spp_(false), sie_(false) {}

uint32_t CSR::read_mstatus() const { return mstatus_; }
void CSR::write_mstatus(uint32_t v) { mstatus_ = v; }

uint32_t CSR::read_mepc() const { return mepc_; }
void CSR::write_mepc(uint32_t v) { mepc_ = v; }

uint32_t CSR::read_mcause() const { return mcause_; }
void CSR::write_mcause(uint32_t v) { mcause_ = v; }

uint32_t CSR::read_mtvec() const { return mtvec_; }
void CSR::write_mtvec(uint32_t v) { mtvec_ = v; }

uint32_t CSR::read(uint32_t csr_addr) const {
  switch (csr_addr) {
    case 0x300: {
      // mstatus is privileged: U-mode reads/writes are illegal
      if (privilege_ == 0) throw CSR::CSRException(2);
      // compose a minimal mstatus view: include raw mstatus bits and SIE as bit 1
      uint32_t v = mstatus_;
      if (sie_) v |= MSTATUS_SIE; else v &= ~MSTATUS_SIE;
      return v;
    }
    case 0x304: {
      uint32_t v = 0;
      if (mie_timer_) v |= (1u << 7);
      if (mie_uart_) v |= (1u << 3);
      return v;
    }
    case 0x305: return read_mtvec();
    case 0x105: return stvec_;
    case 0x141: return sepc_;
    case 0x142: return scause_;
    case 0x143: return stval_;
    case 0x302: return medeleg_;
    case 0x303: return mideleg_;
    case 0x180: return satp_;
    case 0x341: return read_mepc();
    case 0x342: return read_mcause();
    default: return 0;
  }
}

void CSR::write(uint32_t csr_addr, uint32_t value) {
  switch (csr_addr) {
    case 0x300: write_mstatus(value); break;
    case 0x304: set_mie_timer((value >> 7) & 1u); set_mie_uart((value >> 3) & 1u); break;
    case 0x305: write_mtvec(value); break;
    case 0x105: stvec_ = value; break;
    case 0x141: sepc_ = value; break;
    case 0x142: scause_ = value; break;
    case 0x143: stval_ = value; break;
    case 0x302: medeleg_ = value; break;
    case 0x303: mideleg_ = value; break;
    case 0x180: satp_ = value; break;
    case 0x341: write_mepc(value); break;
    case 0x342: write_mcause(value); break;
    default: break;
  }
}

void CSR::set_mstatus_mie(bool en) {
  if (en)
    mstatus_ |= MSTATUS_MIE;
  else
    mstatus_ &= ~MSTATUS_MIE;
}

bool CSR::get_mstatus_mie() const { return (mstatus_ & MSTATUS_MIE) != 0; }

void CSR::set_mie_timer(bool en) { mie_timer_ = en; }
bool CSR::get_mie_timer() const { return mie_timer_; }

void CSR::set_mip_timer(bool pending) { mip_timer_ = pending; }
bool CSR::get_mip_timer() const { return mip_timer_; }

bool CSR::pending_timer_interrupt() const {
  return get_mstatus_mie() && mie_timer_ && mip_timer_;
}

void CSR::set_mie_uart(bool en) { mie_uart_ = en; }
bool CSR::get_mie_uart() const { return mie_uart_; }

void CSR::set_mip_uart(bool pending) { mip_uart_ = pending; }
bool CSR::get_mip_uart() const { return mip_uart_; }

bool CSR::pending_uart_interrupt() const {
  return get_mstatus_mie() && mie_uart_ && mip_uart_;
}

// trap/delegation support
uint32_t CSR::handle_trap(uint32_t cause_raw, uint32_t tval, uint32_t epc, bool is_interrupt) {
  // cause_raw may include interrupt bit in MSB (bit 31)
  bool intr = ((cause_raw >> 31) & 1u) != 0u || is_interrupt;
  uint32_t cause = cause_raw & 0x7fffffffu;
  int prev = privilege_;

  // delegation: exceptions use medeleg, interrupts use mideleg
  bool delegated = false;
  if (intr) {
    if (((mideleg_ >> cause) & 1u) && prev <= 1) delegated = true;
  } else {
    if (((medeleg_ >> cause) & 1u) && prev <= 1) delegated = true;
  }

  if (delegated) {
    // enter S-mode trap
    sepc_ = epc;
    scause_ = cause_raw;
    stval_ = tval;
    // save SIE -> SPIE and clear SIE
    spie_ = sie_;
    sie_ = false;
    // SPP = previous privilege (encode as bool)
    spp_ = (prev == 1);
    // set current privilege to S
    privilege_ = 1;
    return stvec_;
  } else {
    // machine trap entry
    mepc_ = epc;
    mcause_ = cause_raw;
    // save global MIE to MPIE and clear MIE
    mpie_ = get_mstatus_mie();
    set_mstatus_mie(false);
    mpp_ = prev;
    privilege_ = 3; // M-mode
    std::cerr << "CSR: machine trap entry prev=" << prev << " mepc=0x" << std::hex << mepc_ << std::dec << " mpie=" << mpie_ << " mpp=" << mpp_ << " mtvec=0x" << std::hex << mtvec_ << std::dec;
    std::cerr << " cause=0x" << std::hex << cause_raw << " tval=0x" << tval << std::dec << "\n";
    return mtvec_;
  }
}

// privilege and SUM support (basic)
int CSR::get_privilege() const { return privilege_; }
void CSR::set_privilege(int p) { privilege_ = p; }
void CSR::set_sum(bool en) { sum_ = en; }
bool CSR::get_sum() const { return sum_; }

void CSR::enter_trap_to_machine() {
  mpp_ = privilege_;
  mpie_ = get_mstatus_mie();
  set_mstatus_mie(false);
  privilege_ = 3;
}

void CSR::mret_restore() {
  // restore privilege and MIE
  std::cerr << "CSR: mret_restore before mpp=" << mpp_ << " mpie=" << mpie_ << " mstatus_mie=" << get_mstatus_mie() << "\n";
  privilege_ = static_cast<int>(mpp_);
  set_mstatus_mie(mpie_);
  mpie_ = false;
  mpp_ = 0;
  std::cerr << "CSR: mret_restore after privilege=" << privilege_ << " mstatus_mie=" << get_mstatus_mie() << "\n";
}

void CSR::sret_restore() {
  privilege_ = spp_ ? 1 : 0;
  sie_ = spie_;
  spie_ = false;
  spp_ = false;
}

// mstatus helpers (MPP/MPIE/SPP/SPIE/SIE)
uint32_t CSR::get_mpp() const { return mpp_; }
void CSR::set_mpp(uint32_t v) { mpp_ = v; }
bool CSR::get_mpie() const { return mpie_; }
void CSR::set_mpie(bool v) { mpie_ = v; }
bool CSR::get_spie() const { return spie_; }
void CSR::set_spie(bool v) { spie_ = v; }
bool CSR::get_spp() const { return spp_; }
void CSR::set_spp(bool v) { spp_ = v; }
bool CSR::get_mstatus_sie() const { return sie_; }
void CSR::set_mstatus_sie(bool v) { sie_ = v; }

} // namespace cpu
