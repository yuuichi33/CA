typedef unsigned int uint32_t;
volatile int tohost = 0;
volatile uint32_t* const UART = (volatile uint32_t*)0x10001000u;

static void uart_putc(char c) { *UART = (uint32_t)c; }
static void uart_puts(const char* s) { while (*s) uart_putc(*s++); }

int main(void) {
  uart_puts("Hello from benchmark\n");
  tohost = 1;
  return 0;
}
