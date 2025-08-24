#include <stdint.h>
#include <stddef.h>

void box_blur_1d_u8(const uint8_t* in, uint8_t* out, size_t n) {
  if (n < 3) return;
  for (size_t i = 1; i + 1 < n; ++i) {
    unsigned s = in[i - 1] + in[i] + in[i + 1];
    out[i] = (uint8_t)(s / 3);
  }
}
