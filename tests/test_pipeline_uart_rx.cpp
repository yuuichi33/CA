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
  for (int i = 0; i < 16; ++i) prog.push_back(encodeI(0, 0, 0x0, 0, 0x13));

  Pipeline p(prog);

  p.csr().write_mtvec(0x200);
  p.csr().set_mstatus_mie(true);
  p.csr().set_mie_timer(false);
  p.csr().set_mie_uart(true);

  // enable UART RX IRQ in device IER, then inject RX byte
  p.uart_write_ier(2);
  p.inject_uart_rx(static_cast<uint8_t>('Z'));

  bool saw_intr = false;
  for (int i = 0; i < 10; ++i) {
    p.step();
    if (p.pc() == 0x200) { saw_intr = true; break; }
  }

  if (!saw_intr) {
    std::cerr << "uart rx interrupt not taken, pc=" << p.pc() << "\n";
    return 1;
  }

  std::cout << "uart rx interrupt handled, pc=" << p.pc() << std::endl;
  return 0;
}
