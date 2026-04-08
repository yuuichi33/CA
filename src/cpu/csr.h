#pragma once
#include <cstdint>
#include <exception>

namespace cpu {

class CSR {
public:
  CSR();

  uint32_t read_mstatus() const;
  void write_mstatus(uint32_t v);

  uint32_t read_mepc() const;
  void write_mepc(uint32_t v);

  uint32_t read_mcause() const;
  void write_mcause(uint32_t v);

  uint32_t read_mtvec() const;
  void write_mtvec(uint32_t v);

  void set_mstatus_mie(bool en);
  bool get_mstatus_mie() const;

  void set_mie_timer(bool en);
  bool get_mie_timer() const;

  void set_mip_timer(bool pending);
  bool get_mip_timer() const;

  // true if interrupt is pending and globally enabled
  bool pending_timer_interrupt() const;
  
  // UART interrupt controls
  void set_mie_uart(bool en);
  bool get_mie_uart() const;

  void set_mip_uart(bool pending);
  bool get_mip_uart() const;

  bool pending_uart_interrupt() const;

  // generic CSR read/write for supported addresses
  uint32_t read(uint32_t csr_addr) const;
  void write(uint32_t csr_addr, uint32_t value);

  // trap/delegation support
  // handle a trap (exception or interrupt). Returns the new PC (vector) to jump to.
  uint32_t handle_trap(uint32_t cause_raw, uint32_t tval, uint32_t epc, bool is_interrupt);

  // exception thrown on illegal CSR access (to be caught by pipeline)
  class CSRException : public std::exception {
  public:
    explicit CSRException(uint32_t cause) : cause_(cause) {}
    uint32_t cause() const noexcept { return cause_; }
    const char* what() const noexcept override { return "CSR access fault"; }
  private:
    uint32_t cause_;
  };

  // privilege and SUM support (basic)
  // privilege: 0=U,1=S,3=M
  int get_privilege() const;
  void set_privilege(int p);
  void set_sum(bool en);
  bool get_sum() const;
  // trap/return helpers
  void enter_trap_to_machine();
  void mret_restore();
  void sret_restore();
  // mstatus helpers (MPP/MPIE/SPP/SPIE/SIE)
  uint32_t get_mpp() const;
  void set_mpp(uint32_t v);
  bool get_mpie() const;
  void set_mpie(bool v);
  bool get_spie() const;
  void set_spie(bool v);
  bool get_spp() const;
  void set_spp(bool v);
  bool get_mstatus_sie() const;
  void set_mstatus_sie(bool v);

private:
  uint32_t mstatus_;
  uint32_t mepc_;
  uint32_t sepc_;
  uint32_t scause_;
  uint32_t stval_;
  uint32_t mcause_;
  uint32_t mtvec_;
  uint32_t stvec_;
  uint32_t satp_; // satp CSR for page table base/mode
  uint32_t medeleg_;
  uint32_t mideleg_;
  int privilege_; // 0=U,1=S,3=M
  
  bool mie_timer_;
  bool mip_timer_;
  bool mie_uart_;
  bool mip_uart_;
  // additional state for simplified mstatus fields
  bool sum_ = false; // SUM bit
  uint32_t mpp_ = 3; // saved previous privilege on trap (default M)
  bool mpie_ = false; // saved MIE
  bool spie_ = false; // saved SIE
  bool spp_ = false;  // saved previous privilege for SRET
  bool sie_ = false;  // S-mode interrupt enable
};

} // namespace cpu

