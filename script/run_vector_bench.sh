#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RESULTS_DIR="$ROOT/results"
BUILD_DIR="$ROOT/build/bench"
MIBENCH_DIR="$ROOT/third_party/mibench"
PARSEC_DIR="$ROOT/third_party/parsec-benchmark"
TINY_DIR="$ROOT/third_party/tiny-AES-c"

mkdir -p "$RESULTS_DIR" "$BUILD_DIR"

# ---- Compiler selection ----
# Prefer Homebrew LLVM clang (supports -fpass-plugin),
# fall back to system clang if not found.
if [ -x "/opt/homebrew/opt/llvm/bin/clang" ]; then
  CC="/opt/homebrew/opt/llvm/bin/clang"
else
  CC="${CC:-clang}"
fi
echo "[info] Using CC = $CC"
$CC --version | head -n1 || true

# ---- Check VecOpt pass ----
VECOPT_SO="$ROOT/build/VecOpt.dylib"
if [ ! -f "$VECOPT_SO" ]; then
  echo "ERROR: VecOpt.dylib not found at $VECOPT_SO"
  echo "Please build VecOpt.dylib first in $ROOT/build with CMake/Make"
  exit 1
fi

# ---- Common flags ----
# CFLAGS: -std=gnu89 + GNU extensions for old C benchmarks
# LDFLAGS: link with math library
# VECOPT_FLAGS: enable VecOpt plugin and show LLVM vectorizer remarks
CFLAGS="-O3 -std=gnu89 -fheinous-gnu-extensions"
LDFLAGS="-lm"
VECOPT_FLAGS="-fpass-plugin=$VECOPT_SO -Rpass=loop-vectorize -Rpass=slp-vectorizer"

# ---- Timing function ----
# On macOS, /usr/bin/time with -p gives "real X.XXX"
run_and_time() {
  { /usr/bin/time -p "$@" 1>/dev/null; } 2>&1 | awk '/^real/ {print $2}'
}

# =====================================================================
# ============== MiBench: automotive/qsort =============================
# =====================================================================
echo "== MiBench: automotive/qsort =="

QSORT_DIR="$MIBENCH_DIR/automotive/qsort"
QSORT_CSV="$RESULTS_DIR/qsort_times.csv"
: > "$QSORT_CSV"
echo "benchmark,variant,run_idx,seconds" >> "$QSORT_CSV"

mkdir -p "$BUILD_DIR/qsort"
pushd "$BUILD_DIR/qsort" >/dev/null

# Build baseline (-O3) and VecOpt variant
$CC $CFLAGS "$QSORT_DIR/qsort_large.c" $LDFLAGS -o qsort_baseline
$CC $CFLAGS $VECOPT_FLAGS "$QSORT_DIR/qsort_large.c" $LDFLAGS -o qsort_vecopt

QSORT_INPUT="$QSORT_DIR/input_large.dat"

REPS=20
for i in $(seq 1 $REPS); do
  SEC="$(run_and_time ./qsort_baseline "$QSORT_INPUT")"
  echo "qsort,baseline,$i,$SEC" >> "$QSORT_CSV"
done
for i in $(seq 1 $REPS); do
  SEC="$(run_and_time ./qsort_vecopt "$QSORT_INPUT")"
  echo "qsort,vecopt,$i,$SEC" >> "$QSORT_CSV"
done
popd >/dev/null

# Calculate averages
QSORT_BASELINE_AVG=$(awk -F, '$1=="qsort" && $2=="baseline"{s+=$4;n++} END{if(n) printf "%.4f", s/n; else print "n/a"}' "$QSORT_CSV")
QSORT_VECOPT_AVG=$(awk -F, '$1=="qsort" && $2=="vecopt"{s+=$4;n++} END{if(n) printf "%.4f", s/n; else print "n/a"}' "$QSORT_CSV")
echo "qsort baseline avg: $QSORT_BASELINE_AVG s"
echo "qsort vecopt   avg: $QSORT_VECOPT_AVG s"
echo ""

# =====================================================================
# ============== MiBench: automotive/basicmath =========================
# =====================================================================
echo "== MiBench: basicmath_large =="

BASICMATH_DIR="$MIBENCH_DIR/automotive/basicmath"
BASICMATH_CSV="$RESULTS_DIR/basicmath_times.csv"
: > "$BASICMATH_CSV"
echo "benchmark,variant,run_idx,seconds" >> "$BASICMATH_CSV"

mkdir -p "$BUILD_DIR/basicmath"
pushd "$BUILD_DIR/basicmath" >/dev/null

# Build baseline and VecOpt variant
$CC $CFLAGS "$BASICMATH_DIR/basicmath_large.c" \
            "$BASICMATH_DIR/cubic.c" \
            "$BASICMATH_DIR/isqrt.c" \
            "$BASICMATH_DIR/rad2deg.c" \
            $LDFLAGS -o basicmath_baseline

$CC $CFLAGS $VECOPT_FLAGS "$BASICMATH_DIR/basicmath_large.c" \
            "$BASICMATH_DIR/cubic.c" \
            "$BASICMATH_DIR/isqrt.c" \
            "$BASICMATH_DIR/rad2deg.c" \
            $LDFLAGS -o basicmath_vecopt

REPS=50
for i in $(seq 1 $REPS); do
  SEC="$(run_and_time ./basicmath_baseline)"
  echo "basicmath,baseline,$i,$SEC" >> "$BASICMATH_CSV"
done
for i in $(seq 1 $REPS); do
  SEC="$(run_and_time ./basicmath_vecopt)"
  echo "basicmath,vecopt,$i,$SEC" >> "$BASICMATH_CSV"
done
popd >/dev/null

# Calculate averages
BASICMATH_BASELINE_AVG=$(awk -F, '$1=="basicmath" && $2=="baseline"{s+=$4;n++} END{if(n) printf "%.4f", s/n; else print "n/a"}' "$BASICMATH_CSV")
BASICMATH_VECOPT_AVG=$(awk -F, '$1=="basicmath" && $2=="vecopt"{s+=$4;n++} END{if(n) printf "%.4f", s/n; else print "n/a"}' "$BASICMATH_CSV")
echo "basicmath baseline avg: $BASICMATH_BASELINE_AVG s"
echo "basicmath vecopt   avg: $BASICMATH_VECOPT_AVG s"
echo ""

# =====================================================================
# ============== PARSEC: blackscholes =================================
# =====================================================================
echo "== PARSEC: blackscholes =="

BLACKSCH_SRC="$PARSEC_DIR/pkgs/apps/blackscholes/src/blackscholes.c"
INPUT_DIR="$PARSEC_DIR/pkgs/apps/blackscholes/inputs"

# Auto-extract *.tar input files if they exist
if ls "$INPUT_DIR"/*.tar >/dev/null 2>&1; then
  for tarf in "$INPUT_DIR"/*.tar; do
    echo "[info] extracting $(basename "$tarf")"
    tar -xf "$tarf" -C "$INPUT_DIR"
  done
fi

# Prefer in_16.txt, otherwise use first in_*.txt
if [ -f "$INPUT_DIR/in_16.txt" ]; then
  BLACKSCH_INPUT="$INPUT_DIR/in_16.txt"
else
  BLACKSCH_INPUT="$(ls "$INPUT_DIR"/in_*.txt 2>/dev/null | head -n1 || true)"
fi

if [ ! -f "$BLACKSCH_INPUT" ]; then
  echo "ERROR: blackscholes input not found under $INPUT_DIR"
  exit 1
fi

BLACKSCH_OUTPUT="out.txt"
BLACKSCH_CSV="$RESULTS_DIR/blackscholes_times.csv"
: > "$BLACKSCH_CSV"
echo "benchmark,variant,run_idx,seconds" >> "$BLACKSCH_CSV"

mkdir -p "$BUILD_DIR/blackscholes"
pushd "$BUILD_DIR/blackscholes" >/dev/null

# Build baseline and VecOpt variant
CFLAGS_PTHREADS="$CFLAGS -pthread"
$CC $CFLAGS_PTHREADS "$BLACKSCH_SRC" $LDFLAGS -o blackscholes_baseline
$CC $CFLAGS_PTHREADS $VECOPT_FLAGS "$BLACKSCH_SRC" $LDFLAGS -o blackscholes_vecopt

REPS=5
for i in $(seq 1 $REPS); do
  SEC="$(run_and_time ./blackscholes_baseline 1 "$BLACKSCH_INPUT" "$BLACKSCH_OUTPUT")"
  echo "blackscholes,baseline,$i,$SEC" >> "$BLACKSCH_CSV"
done
for i in $(seq 1 $REPS); do
  SEC="$(run_and_time ./blackscholes_vecopt 1 "$BLACKSCH_INPUT" "$BLACKSCH_OUTPUT")"
  echo "blackscholes,vecopt,$i,$SEC" >> "$BLACKSCH_CSV"
done
popd >/dev/null

# Calculate averages
BLACKSCH_BASELINE_AVG=$(awk -F, '$1=="blackscholes" && $2=="baseline"{s+=$4;n++} END{if(n) printf "%.4f", s/n; else print "n/a"}' "$BLACKSCH_CSV")
BLACKSCH_VECOPT_AVG=$(awk -F, '$1=="blackscholes" && $2=="vecopt"{s+=$4;n++} END{if(n) printf "%.4f", s/n; else print "n/a"}' "$BLACKSCH_CSV")
echo "blackscholes baseline avg: $BLACKSCH_BASELINE_AVG s"
echo "blackscholes vecopt   avg: $BLACKSCH_VECOPT_AVG s"
echo ""

# =====================================================================
# ============== tiny-AES-c ===========================================
# =====================================================================
echo "== tiny-AES-c (test.c + aes.c) =="

if [ ! -d "$TINY_DIR/.git" ]; then
  mkdir -p "$TINY_DIR"
  git clone --depth=1 https://github.com/kokke/tiny-AES-c "$TINY_DIR"
fi

AES_CSV="$RESULTS_DIR/tinyaes_times.csv"
: > "$AES_CSV"
echo "benchmark,variant,run_idx,seconds" >> "$AES_CSV"

mkdir -p "$BUILD_DIR/tiny-aes"
pushd "$BUILD_DIR/tiny-aes" >/dev/null

# Build baseline and VecOpt variant
$CC $CFLAGS -I"$TINY_DIR" "$TINY_DIR/test.c" "$TINY_DIR/aes.c" $LDFLAGS -o aes_baseline
$CC $CFLAGS $VECOPT_FLAGS -I"$TINY_DIR" "$TINY_DIR/test.c" "$TINY_DIR/aes.c" $LDFLAGS -o aes_vecopt

REPS=10
for i in $(seq 1 $REPS); do
  SEC="$(run_and_time ./aes_baseline)"
  echo "tinyaes,baseline,$i,$SEC" >> "$AES_CSV"
done
for i in $(seq 1 $REPS); do
  SEC="$(run_and_time ./aes_vecopt)"
  echo "tinyaes,vecopt,$i,$SEC" >> "$AES_CSV"
done
popd >/dev/null

# Calculate averages
AES_BASELINE_AVG=$(awk -F, '$1=="tinyaes" && $2=="baseline"{s+=$4;n++} END{if(n) printf "%.4f", s/n; else print "n/a"}' "$AES_CSV")
AES_VECOPT_AVG=$(awk -F, '$1=="tinyaes" && $2=="vecopt"{s+=$4;n++} END{if(n) printf "%.4f", s/n; else print "n/a"}' "$AES_CSV")
echo "tinyaes baseline avg: $AES_BASELINE_AVG s"
echo "tinyaes vecopt   avg: $AES_VECOPT_AVG s"
echo ""

# =====================================================================
# ============== Done =================================================
# =====================================================================
echo "All benchmarks finished."
echo "Results saved to:"
echo "  - $QSORT_CSV"
echo "  - $BASICMATH_CSV"
echo "  - $BLACKSCH_CSV"
echo "  - $AES_CSV"
