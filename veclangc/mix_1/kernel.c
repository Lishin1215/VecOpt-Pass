int compute_sqdist(int *xs, int *ys, int *zs, int *dists, int count) {
  int i;
  i = 0;
  while (i < count) {
    int xi;
    int yi;
    int zi;
    xi = xs[i];
    yi = ys[i];
    zi = zs[i];
    dists[i] = xi * xi + yi * yi + zi * zi;
    i = i + 1;
  }
  return 0;
}