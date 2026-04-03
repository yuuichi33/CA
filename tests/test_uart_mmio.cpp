#include <iostream>
#include "mem/memory.h"
#include "periph/uart_mmio.h"

int main() {
  constexpr uint32_t BASE = 0x10001000u;
  mem::Memory m(4096);
  auto *uart = new periph::UARTMMIO();
  m.map_device(BASE, 0x10, uart);

  m.store32(BASE + 0x0, static_cast<uint32_t>('H'));
  m.store32(BASE + 0x0, static_cast<uint32_t>('i'));

  const auto &buf = uart->tx_buffer();
  if (buf.size() != 2 || buf[0] != 'H' || buf[1] != 'i') {
    std::cerr << "uart mmio failed\n";
    return 1;
  }

  std::cout << "uart mmio ok\n";
  return 0;
}
