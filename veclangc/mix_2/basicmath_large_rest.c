extern int isqrt_int(int n);
#include "snipmath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXARRAY 5

int main(void)
{
  double  a1 = 1.0, b1 = -10.5, c1 = 32.0, d1 = -30.0;
  double  x[3];
  double  X;
  int     solutions;
  int i;
  unsigned long l = 0x3fed0169UL;

  printf("********* CUBIC FUNCTIONS ***********\n");

  SolveCubic(a1, b1, c1, d1, &solutions, x);
  printf("Solutions:");
  for (i = 0; i < solutions; i++) printf(" %f", x[i]);
  printf("\n");

  a1 = 1.0; b1 = -4.5; c1 = 17.0; d1 = -30.0;
  SolveCubic(a1, b1, c1, d1, &solutions, x);
  printf("Solutions:");
  for (i = 0; i < solutions; i++) printf(" %f", x[i]);
  printf("\n");

  a1 = 1.0; b1 = -3.5; c1 = 22.0; d1 = -31.0;
  SolveCubic(a1, b1, c1, d1, &solutions, x);
  printf("Solutions:");
  for (i = 0; i < solutions; i++) printf(" %f", x[i]);
  printf("\n");

  a1 = 1.0; b1 = -13.7; c1 = 1.0; d1 = -35.0;
  SolveCubic(a1, b1, c1, d1, &solutions, x);
  printf("Solutions:");
  for (i = 0; i < solutions; i++) printf(" %f", x[i]);
  printf("\n");

  a1 = 3.0; b1 = 12.34; c1 = 5.0; d1 = 12.0;
  SolveCubic(a1, b1, c1, d1, &solutions, x);
  printf("Solutions:");
  for (i = 0; i < solutions; i++) printf(" %f", x[i]);
  printf("\n");

  a1 = -8.0; b1 = -67.89; c1 = 6.0; d1 = -23.6;
  SolveCubic(a1, b1, c1, d1, &solutions, x);
  printf("Solutions:");
  for (i = 0; i < solutions; i++) printf(" %f", x[i]);
  printf("\n");

  a1 = 45.0; b1 = 8.67; c1 = 7.5; d1 = 34.0;
  SolveCubic(a1, b1, c1, d1, &solutions, x);
  printf("Solutions:");
  for (i = 0; i < solutions; i++) printf(" %f", x[i]);
  printf("\n");

  a1 = -12.0; b1 = -1.7; c1 = 5.3; d1 = 16.0;
  SolveCubic(a1, b1, c1, d1, &solutions, x);
  printf("Solutions:");
  for (i = 0; i < solutions; i++) printf(" %f", x[i]);
  printf("\n");

  // Test a range of cubic equations with varying coefficients
  for (a1 = 1; a1 < 10; a1 += 1) {
    for (b1 = 10; b1 > 0; b1 -= .25) {
      for (c1 = 5; c1 < 15; c1 += 0.61) {
        for (d1 = -1; d1 > -5; d1 -= .451) {
          SolveCubic(a1, b1, c1, d1, &solutions, x);
          printf("Solutions:");
          for (i = 0; i < solutions; i++) printf(" %f", x[i]);
          printf("\n");
        }
      }
    }
  }

  printf("********* INTEGER SQUARE ROOTS ***********\n");

  // Test integer square root for a range of values
  for (i = 0; i < 100000; i += 2) {
    int r = isqrt_int(i);
    printf("sqrt(%3d) = %2d\n", i, r);
  }
  printf("\n");

  // Test integer square root for a range of hexadecimal values
  for (l = 0x3fed0169UL; l < 0x3fed4169UL; l++) {
    int r = isqrt_int((int)l);
    printf("sqrt(%lX) = %X\n", l, r);
  }

  printf("********* ANGLE CONVERSION ***********\n");
  // Convert degrees to radians
  for (X = 0.0; X <= 360.0; X += .001)
    printf("%3.0f degrees = %.12f radians\n", X, deg2rad(X));
  puts("");
  // Convert radians to degrees
  for (X = 0.0; X <= (2 * PI + 1e-6); X += (PI / 5760))
    printf("%.12f radians = %3.0f degrees\n", X, rad2deg(X));

  return 0;
}
