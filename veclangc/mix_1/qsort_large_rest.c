#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "kernel.h"

#define MAXARRAY 5

struct my3DVertexStruct {
  int x, y, z;
  double distance;
};

int compare(const void *elem1, const void *elem2)
{
  double distance1 = ((const struct my3DVertexStruct *)elem1)->distance;
  double distance2 = ((const struct my3DVertexStruct *)elem2)->distance;
  return (distance1 > distance2) ? 1 : ((distance1 == distance2) ? 0 : -1);
}

int main(int argc, char *argv[]) {
  struct my3DVertexStruct *array = malloc(sizeof(*array) * MAXARRAY);
  int *xs = malloc(sizeof(int) * MAXARRAY);
  int *ys = malloc(sizeof(int) * MAXARRAY);
  int *zs = malloc(sizeof(int) * MAXARRAY);
  int *dists = malloc(sizeof(int) * MAXARRAY);
  if (!array || !xs || !ys || !zs || !dists) return 2;

  FILE *fp;
  int i, count = 0;
  int x, y, z;

  if (argc < 2) {
    fprintf(stderr, "Usage: qsort_large <file>\n");
    return 1;
  }

  fp = fopen(argv[1], "r");
  if (!fp) { perror("fopen"); return 1; }

  while ((fscanf(fp, "%d", &x) == 1) &&
         (fscanf(fp, "%d", &y) == 1) &&
         (fscanf(fp, "%d", &z) == 1) &&
         (count < MAXARRAY)) {
    xs[count] = x;
    ys[count] = y;
    zs[count] = z;
    array[count].x = x;
    array[count].y = y;
    array[count].z = z;
    count++;
  }
  fclose(fp);

  compute_sqdist(xs, ys, zs, dists, count);

  for (i = 0; i < count; i++) array[i].distance = (double)dists[i];

  printf("\nSorting %d vectors based on distance from the origin.\n\n", count);
  qsort(array, count, sizeof(struct my3DVertexStruct), compare);

  for (i = 0; i < count; i++)
    printf("%d %d %d\n", array[i].x, array[i].y, array[i].z);

  free(array); free(xs); free(ys); free(zs); free(dists);
  return 0;
}