#include "cpu/csr.h"

namespace cpu {

// mstatus MIE bit position (for our simple simulator)
static constexpr uint32_t MSTATUS_MIE = (1u << 3);

CSR::CSR()
    : mstatus_(0), mepc_(0), mcause_(0), mtvec_(0), mie_timer_(false), mip_timer_(false) {}

uint32_t CSR::read_mstatus() const { return mstatus_; }
void CSR::write_mstatus(uint32_t v) { mstatus_ = v; }

uint32_t CSR::read_mepc() const { return mepc_; }
void CSR::write_mepc(uint32_t v) { mepc_ = v; }

uint32_t CSR::read_mcause() const { return mcause_; }
void CSR::write_mcause(uint32_t v) { mcause_ = v; }

uint32_t CSR::read_mtvec() const { return mtvec_; }
void CSR::write_mtvec(uint32_t v) { mtvec_ = v; }

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

} // namespace cpu
