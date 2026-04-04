#include "cpu/csr.h"

namespace cpu {

// mstatus MIE bit position (for our simple simulator)
static constexpr uint32_t MSTATUS_MIE = (1u << 3);

CSR::CSR()
  : mstatus_(0), mepc_(0), mcause_(0), mtvec_(0), mie_timer_(false), mip_timer_(false), mie_uart_(false), mip_uart_(false) {}

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
    case 0x300: return read_mstatus();
    case 0x304: {
      uint32_t v = 0;
      if (mie_timer_) v |= (1u << 7);
      if (mie_uart_) v |= (1u << 3);
      return v;
    }
    case 0x305: return read_mtvec();
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

} // namespace cpu
