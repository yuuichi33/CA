#pragma once
#include <cstdint>

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

private:
  uint32_t mstatus_;
  uint32_t mepc_;
  uint32_t mcause_;
  uint32_t mtvec_;
  bool mie_timer_;
  bool mip_timer_;
  bool mie_uart_;
  bool mip_uart_;
};

} // namespace cpu

