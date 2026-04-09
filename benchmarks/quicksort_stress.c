typedef unsigned int uint32_t;
typedef int int32_t;

volatile int tohost = 0;
volatile uint32_t* const UART = (volatile uint32_t*)0x10001000u;

enum {
  QUICKSORT_N = 4096,
  QUICKSORT_ROUNDS = 3,
  INSERTION_THRESHOLD = 16
};

static int32_t data_buf[QUICKSORT_N];
static uint32_t rng_state = 0x12345678u;

static void uart_putc(char c) {
  *UART = (uint32_t)(unsigned char)c;
}

static void uart_puts(const char* s) {
  while (*s) uart_putc(*s++);
}

static void uart_put_hex32(uint32_t v) {
  static const char* HEX = "0123456789abcdef";
  for (int i = 7; i >= 0; --i) {
    uart_putc(HEX[(v >> (i * 4)) & 0xfu]);
  }
}

static uint32_t xorshift32(void) {
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}

static void fill_random_data(void) {
  for (int i = 0; i < QUICKSORT_N; ++i) {
    data_buf[i] = (int32_t)xorshift32();
  }
}

static void swap_i32(int32_t* a, int32_t* b) {
  int32_t t = *a;
  *a = *b;
  *b = t;
}

static int median3_index(int lo, int hi) {
  int mid = lo + ((hi - lo) >> 1);
  int32_t a = data_buf[lo];
  int32_t b = data_buf[mid];
  int32_t c = data_buf[hi];

  if (a < b) {
    if (b < c) return mid;
    if (a < c) return hi;
    return lo;
  }
  if (a < c) return lo;
  if (b < c) return hi;
  return mid;
}

static int partition_hoare(int lo, int hi) {
  int32_t pivot = data_buf[median3_index(lo, hi)];
  int i = lo;
  int j = hi;

  while (i <= j) {
    while (data_buf[i] < pivot) ++i;
    while (data_buf[j] > pivot) --j;
    if (i <= j) {
      swap_i32(&data_buf[i], &data_buf[j]);
      ++i;
      --j;
    }
  }
  return i;
}

static void insertion_sort_range(int lo, int hi) {
  for (int i = lo + 1; i <= hi; ++i) {
    int32_t key = data_buf[i];
    int j = i - 1;
    while (j >= lo && data_buf[j] > key) {
      data_buf[j + 1] = data_buf[j];
      --j;
    }
    data_buf[j + 1] = key;
  }
}

static void quicksort_recursive(int lo, int hi) {
  if (lo >= hi) return;

  if ((hi - lo) <= INSERTION_THRESHOLD) {
    insertion_sort_range(lo, hi);
    return;
  }

  int cut = partition_hoare(lo, hi);
  if (lo < (cut - 1)) quicksort_recursive(lo, cut - 1);
  if (cut < hi) quicksort_recursive(cut, hi);
}

static int is_sorted(void) {
  for (int i = 1; i < QUICKSORT_N; ++i) {
    if (data_buf[i - 1] > data_buf[i]) return 0;
  }
  return 1;
}

static uint32_t checksum(void) {
  uint32_t h = 0x9e3779b9u;
  for (int i = 0; i < QUICKSORT_N; ++i) {
    h ^= ((uint32_t)data_buf[i] + 0x9e3779b9u + (h << 6) + (h >> 2));
  }
  return h;
}

int main(void) {
  uart_puts("quicksort_stress start\n");

  uint32_t final = 0;
  for (int round = 0; round < QUICKSORT_ROUNDS; ++round) {
    fill_random_data();
    quicksort_recursive(0, QUICKSORT_N - 1);
    if (!is_sorted()) {
      uart_puts("quicksort_stress failed\n");
      tohost = 2;
      return 2;
    }
    final ^= checksum();
  }

  uart_puts("quicksort_stress ok checksum=0x");
  uart_put_hex32(final);
  uart_putc('\n');

  tohost = 1;
  return 0;
}
