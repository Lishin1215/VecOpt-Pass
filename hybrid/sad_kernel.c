#include "include/sad.h"

int sad(const int *a, const int *b, int n) {
  int s = 0;
  for (int i = 0; i < n; i++) {
    int d = a[i] - b[i];
    if (d < 0) d = -d;
    s += d;
  }
  return s;
}
