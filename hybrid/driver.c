#include <stdio.h>
#include "include/sad.h"

int main() {
  int a[8] = {1,2,3,4,5,6,7,8};
  int b[8] = {2,2,2,2,2,2,2,2};
  // SAD = |1-2| + |2-2| + ... + |8-2| = 1 + 0 + 1 + 2 + 3 + 4 + 5 + 6 = 22
  int s = sad(a, b, 8);
  printf("%d\n", s);
  return 0;
}
