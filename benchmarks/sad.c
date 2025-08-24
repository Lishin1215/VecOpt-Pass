#include <stdint.h>
#include <stdlib.h>

int sad_u8(const uint8_t* a, const uint8_t* b, size_t n) {
  int acc = 0;
  for (size_t i = 0; i < n; ++i) {
    int d = (int)a[i] - (int)b[i];
    acc += d < 0 ? -d : d;
  }
  return acc;
}
