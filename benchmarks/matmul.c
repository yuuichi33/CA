#include <stddef.h>
typedef unsigned int uint32_t;
typedef int int32_t;

volatile int tohost = 0;
volatile uint32_t* const UART = (volatile uint32_t*)0x10001000u;

int __divsi3(int a, int b) {
  if (b == 0) return 0;
  int sign = (a < 0) ^ (b < 0);
  unsigned int ua = (a < 0) ? (unsigned int)(-a) : (unsigned int)a;
  unsigned int ub = (b < 0) ? (unsigned int)(-b) : (unsigned int)b;
  unsigned int q = 0;
  unsigned int r = 0;
  for (int i = 31; i >= 0; --i) {
    r = (r << 1) | ((ua >> i) & 1u);
    if (r >= ub) { r -= ub; q |= (1u << i); }
  }
  int res = (int)q;
  return sign ? -res : res;
}

int __modsi3(int a, int b) {
  if (b == 0) return 0;
  unsigned int ua = (a < 0) ? (unsigned int)(-a) : (unsigned int)a;
  unsigned int ub = (b < 0) ? (unsigned int)(-b) : (unsigned int)b;
  unsigned int r = 0;
  for (int i = 31; i >= 0; --i) {
    r = (r << 1) | ((ua >> i) & 1u);
    if (r >= ub) { r -= ub; }
  }
  int rem = (int)r;
  return (a < 0) ? -rem : rem;
}

static void uart_putc(char c) {
  *UART = (uint32_t)c;
}

static void uart_puts(const char* s) {
  while (*s) {
    uart_putc(*s++);
  }
}

int mul_int(int a, int b) {
  unsigned int ua = (a < 0) ? (unsigned int)(-a) : (unsigned int)a;
  unsigned int ub = (b < 0) ? (unsigned int)(-b) : (unsigned int)b;
  unsigned int res = 0;
  while (ub) {
    if (ub & 1u) res += ua;
    ua <<= 1;
    ub >>= 1;
  }
  int r = (int)res;
  if ((a < 0) ^ (b < 0)) r = -r;
  return r;
}

int main(void) {
  enum { N = 16 };
  int A[N][N];
  int B[N][N];
  int C[N][N];

  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      A[i][j] = (i + 1) * (j + 1);
      B[i][j] = (i + 2) * (j + 3);
      C[i][j] = 0;
    }
  }

  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      int sum = 0;
      for (int k = 0; k < N; ++k) {
        sum += mul_int(A[i][k], B[k][j]);
      }
      C[i][j] = sum;
    }
  }

  // compute a checksum to prevent optimizing away
  volatile int checksum = 0;
  for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) checksum += C[i][j];

  uart_puts("Benchmark Finished\n");
  // also print checksum (decimal)
  // simple itoa
  char buf[16];
  int n = 0;
  int v = checksum;
  if (v == 0) { buf[n++] = '0'; }
  else {
    int tmp = v;
    char rev[16]; int r = 0;
    if (tmp < 0) { tmp = -tmp; }
    while (tmp > 0) { rev[r++] = '0' + (tmp % 10); tmp /= 10; }
    if (v < 0) buf[n++] = '-';
    for (int i = r-1; i >= 0; --i) buf[n++] = rev[i];
  }
  buf[n++] = '\n';
  for (int i = 0; i < n; ++i) uart_putc(buf[i]);

  // signal completion to host semihosting
  tohost = 1;
  return 0;
}
