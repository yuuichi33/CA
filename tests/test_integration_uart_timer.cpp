#include <iostream>
#include <vector>
#include "cpu/pipeline.h"

using namespace cpu;

static uint32_t encodeI(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
  uint32_t uimm = static_cast<uint32_t>(imm) & 0xfff;
  return (uimm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

int main() {
  std::vector<uint32_t> prog;
  for (int i = 0; i < 64; ++i) prog.push_back(encodeI(0, 0, 0x0, 0, 0x13));

  Pipeline p(prog);

  // enable verbose debug for integration debugging
  p.set_verbose(true);

  // install a minimal mtvec handler (MRET) so the trap handler returns and
  // re-enables interrupts; MRET encoding = 0x30200073
  p.memory().store32(0x200, 0x30200073);

  p.csr().write_mtvec(0x200);
  p.csr().set_mstatus_mie(true);
  p.csr().set_mie_timer(true);
  p.csr().set_mie_uart(true);

  p.timer().set_interval(3);
  p.timer().set_enabled(true);
  p.uart_write_ier(2); // enable RX IRQ

  // inject one RX byte
  p.inject_uart_rx(static_cast<uint8_t>('I'));

  uint32_t last_mcause = p.csr().read_mcause();
  int saw_timer = 0;
  int saw_uart = 0;

  for (int i = 0; i < 200; ++i) {
    p.step();
    uint32_t mc = p.csr().read_mcause();
    if (mc != last_mcause) {
      if (mc == ((1u << 31) | 7u)) ++saw_timer;
      if (mc == ((1u << 31) | 3u)) ++saw_uart;
      last_mcause = mc;
    }
  }

  if (saw_timer == 0) { std::cerr << "no timer interrupt seen\n"; return 1; }
  if (saw_uart == 0) { std::cerr << "no uart interrupt seen\n"; return 2; }

  std::cout << "integration ok: timer=" << saw_timer << " uart=" << saw_uart << "\n";
  return 0;
}
