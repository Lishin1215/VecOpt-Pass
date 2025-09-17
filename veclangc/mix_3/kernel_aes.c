int subbytes_kernel(int* sbox, int* in, int* out, int n) {
    int i = 0;
    while (i < n) {
        int v = in[i];          
        out[i] = sbox[v];     
        i = i + 1;
    }
    return 0;
}