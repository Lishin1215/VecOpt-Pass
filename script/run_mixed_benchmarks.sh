#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build/mix_test"
VECLANGC="${VECLANGC:-$ROOT/veclangc/veclangc}"
CLANG="${CLANG:-clang}"
VECOPT_SO="${VECOPT_SO:-$ROOT/build/VecOpt.dylib}"
RESULTS="$ROOT/results"
mkdir -p "$BUILD" "$RESULTS"

echo "VECLANGC=$VECLANGC CLANG=$CLANG VECOPT_SO=$VECOPT_SO"
cd "$BUILD"

# 1) tiny-AES-c
if [ ! -d tiny-AES-c ]; then
  git clone --depth=1 https://github.com/kokke/tiny-AES-c tiny-AES-c
fi
pushd tiny-AES-c >/dev/null
cp aes.c aes_test.c "$BUILD"/ || true
# Create small main that uses AES encrypt/decrypt if example not present
cat > "$BUILD/tiny_main.c" <<'CMAIN'
#include <stdio.h>
#include <stdint.h>
#include "aes.h"
int main(void){
  uint8_t key[16] = {0};
  uint8_t in[16]  = {0};
  uint8_t out[16];
  AES_init_ctx((AES_ctx*)&key);
  AES_ECB_encrypt((AES_ctx*)&key, out);
  printf("ok\n");
  return 0;
}
CMAIN
popd >/dev/null

echo "Building tiny-AES-c variants..."
# baseline
$CLANG -O3 -march=native "$BUILD/tiny_main.c" tiny-AES-c/aes.c -Itiny-AES-c -o tiny_baseline 2>/dev/null || true
# vecopt (clang with plugin) if available
if [ -f "$VECOPT_SO" ]; then
  $CLANG -O3 -march=native -fpass-plugin="$VECOPT_SO" -Rpass=loop-vectorize -Rpass=slp-vectorizer \
    "$BUILD/tiny_main.c" tiny-AES-c/aes.c -Itiny-AES-c -o tiny_vecopt 2>/dev/null || true
fi
# mixed: try compile main with veclangc, lib with clang
if [ -x "$VECLANGC" ] || command -v "$VECLANGC" >/dev/null 2>&1; then
  "$VECLANGC" -O3 -c "$BUILD/tiny_main.c" -I"$BUILD/tiny-AES-c" -o tiny_main.o 2>/dev/null || echo "veclangc: tiny_main.c not supported"
  $CLANG -O3 -c tiny-AES-c/aes.c -Itiny-AES-c -o aes.o 2>/dev/null || true
  if [ -f tiny_main.o ] && [ -f aes.o ]; then
    $CLANG tiny_main.o aes.o -o tiny_mixed 2>/dev/null || true
  fi
fi

# 2) xxHash
if [ ! -d xxHash ]; then
  git clone --depth=1 https://github.com/Cyan4973/xxHash xxHash
fi
# create small main that calls XXH64
cat > "$BUILD/xxh_main.c" <<'CXXH'
#include <stdio.h>
#include <stdint.h>
#include "xxhash.h"
int main(void){
  const char *s="hello";
  unsigned long long h = XXH64(s,5,0);
  printf("%llu\n", (unsigned long long)h);
  return 0;
}
CXXH

echo "Building xxHash variants..."
$CLANG -O3 -march=native xxHash/xxhash.c "$BUILD/xxh_main.c" -IxxHash -o xxh_baseline 2>/dev/null || true
if [ -f "$VECOPT_SO" ]; then
  $CLANG -O3 -march=native -fpass-plugin="$VECOPT_SO" xxHash/xxhash.c "$BUILD/xxh_main.c" -IxxHash -o xxh_vecopt 2>/dev/null || true
fi
# mixed: compile main with veclangc, lib with clang
if [ -x "$VECLANGC" ] || command -v "$VECLANGC" >/dev/null 2>&1; then
  $CLANG -O3 -c xxHash/xxhash.c -IxxHash -o xxh_lib.o 2>/dev/null || true
  "$VECLANGC" -O3 -c "$BUILD/xxh_main.c" -IxxHash -o xxh_main.o 2>/dev/null || echo "veclangc: xxh_main.c not supported"
  if [ -f xxh_main.o ] && [ -f xxh_lib.o ]; then
    $CLANG xxh_main.o xxh_lib.o -o xxh_mixed 2>/dev/null || true
  fi
fi

# 3) chibicc / 9cc tests (compile small tests with veclangc to check compatibility)
if [ ! -d chibicc ]; then
  git clone --depth=1 https://github.com/rui314/chibicc chibicc
fi
mkdir -p chibicc_tests
# collect some small test snippets from chibicc/tests (if present) or create a few small C snippets
if [ -d chibicc/test ]; then
  cp chibicc/test/*.c chibicc_tests/ 2>/dev/null || true
fi
# add a few tiny snippets
cat > chibicc_tests/test1.c <<'T1'
int main(){ int a=1; int b=2; return a+b; }
T1
cat > chibicc_tests/test2.c <<'T2'
int fib(int n){ if(n<2) return n; return fib(n-1)+fib(n-2); }
int main(){ return fib(10); }
T2

echo "Trying to compile chibicc test snippets with veclangc..."
for f in chibicc_tests/*.c; do
  echo -n "veclangc -> $f : "
  if "$VECLANGC" -c "$f" -o /dev/null 2>/dev/null; then
    echo "OK"
  else
    echo "FAIL"
  fi
done

# Quick run of produced binaries (if any)
echo ""
echo "=== Quick run (if binaries exist) ==="
for b in tiny_baseline tiny_vecopt tiny_mixed xxh_baseline xxh_vecopt xxh_mixed; do
  if [ -x "$b" ]; then
    printf "%-15s: " "$b"
    /usr/bin/time -f "%e" ./"$b" 2>&1 1>/dev/null | tail -n1 || true
  fi
done

echo "Done. Artifacts in $BUILD. Results directory: $RESULTS"
echo "If veclangc failed on some files, inspect the logs above and try small kernels instead."