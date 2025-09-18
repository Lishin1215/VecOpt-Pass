#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RESULTS_DIR="$ROOT/results"
BUILD_DIR="$ROOT/build/bench"
MIBENCH_DIR="$ROOT/third_party/mibench"
PARSEC_DIR="$ROOT/third_party/parsec-benchmark"

mkdir -p "$RESULTS_DIR" "$BUILD_DIR"

# ===== check VecOpt =====
VECOPT_SO="$ROOT/build/VecOpt.dylib"
if [ ! -f "$VECOPT_SO" ]; then
  echo "ERROR: VecOpt.so not found at $VECOPT_SO"
  exit 1
fi

CC="clang"
CFLAGS="-O3 -std=gnu89 -fheinous-gnu-extensions"
LDFLAGS="-lm"
VECOPT_FLAGS="-fpass-plugin=$VECOPT_SO -Rpass=loop-vectorize -Rpass=slp-vectorizer"
GTIME="/opt/homebrew/bin/gtime"   # GNU time

# helper: run with gtime and return seconds
run_time() {
  $GTIME -f "%e" "$@" 1>/dev/null 2>&1
}

# ======================================================
# ===== MiBench: qsort =====
# ======================================================
echo "== MiBench: automotive/qsort =="

QSORT_DIR="$MIBENCH_DIR/automotive/qsort"
QSORT_CSV="$RESULTS_DIR/qsort_times.csv"
: > "$QSORT_CSV"
echo "benchmark,variant,run_idx,seconds" >> "$QSORT_CSV"

mkdir -p "$BUILD_DIR/qsort"
pushd "$BUILD_DIR/qsort" >/dev/null

$CC $CFLAGS "$QSORT_DIR/qsort_large.c" $LDFLAGS -o qsort_baseline
$CC $CFLAGS $VECOPT_FLAGS "$QSORT_DIR/qsort_large.c" $LDFLAGS -o qsort_vecopt

QSORT_INPUT="$QSORT_DIR/input_large.dat"

REPS=20
for i in $(seq 1 $REPS); do
  SEC=$(run_time ./qsort_baseline "$QSORT_INPUT")
  echo "qsort,baseline,$i,$SEC" >> "$QSORT_CSV"
done
for i in $(seq 1 $REPS); do
  SEC=$(run_time ./qsort_vecopt "$QSORT_INPUT")
  echo "qsort,vecopt,$i,$SEC" >> "$QSORT_CSV"
done

popd >/dev/null

QSORT_BASELINE_AVG=$(awk -F, '$1=="qsort" && $2=="baseline"{s+=$4;n++} END{printf "%.9f", s/n}' "$QSORT_CSV")
QSORT_VECOPT_AVG=$(awk -F, '$1=="qsort" && $2=="vecopt"{s+=$4;n++} END{printf "%.9f", s/n}' "$QSORT_CSV")
echo "qsort baseline avg: $QSORT_BASELINE_AVG s"
echo "qsort vecopt   avg: $QSORT_VECOPT_AVG s"
echo ""


# ======================================================
# ===== MiBench: basicmath_large =====================
# ======================================================
echo "== MiBench: basicmath_large =="

BASICMATH_DIR="$MIBENCH_DIR/automotive/basicmath"
BASICMATH_CSV="$RESULTS_DIR/basicmath_times.csv"
: > "$BASICMATH_CSV"
echo "benchmark,variant,run_idx,seconds" >> "$BASICMATH_CSV"

mkdir -p "$BUILD_DIR/basicmath"
pushd "$BUILD_DIR/basicmath" >/dev/null

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
  SEC=$(run_time ./basicmath_baseline)
  echo "basicmath,baseline,$i,$SEC" >> "$BASICMATH_CSV"
done
for i in $(seq 1 $REPS); do
  SEC=$(run_time ./basicmath_vecopt)
  echo "basicmath,vecopt,$i,$SEC" >> "$BASICMATH_CSV"
done

popd >/dev/null

BASICMATH_BASELINE_AVG=$(awk -F, '$1=="basicmath" && $2=="baseline"{s+=$4;n++} END{printf "%.9f", s/n}' "$BASICMATH_CSV")
BASICMATH_VECOPT_AVG=$(awk -F, '$1=="basicmath" && $2=="vecopt"{s+=$4;n++} END{printf "%.9f", s/n}' "$BASICMATH_CSV")
echo "basicmath baseline avg: $BASICMATH_BASELINE_AVG s"
echo "basicmath vecopt   avg: $BASICMATH_VECOPT_AVG s"
echo ""


# ======================================================
# ===== PARSEC: blackscholes ==========================
# ======================================================
echo "== PARSEC: blackscholes =="

BLACKSCH_SRC="$PARSEC_DIR/pkgs/apps/blackscholes/src/blackscholes.c"
BLACKSCH_INPUT="$PARSEC_DIR/pkgs/apps/blackscholes/inputs/in_16.txt"
BLACKSCH_OUTPUT="out.txt"
BLACKSCH_CSV="$RESULTS_DIR/blackscholes_times.csv"
: > "$BLACKSCH_CSV"
echo "benchmark,variant,run_idx,seconds" >> "$BLACKSCH_CSV"

mkdir -p "$BUILD_DIR/blackscholes"
pushd "$BUILD_DIR/blackscholes" >/dev/null

CFLAGS_PTHREADS="$CFLAGS -pthread"
$CC $CFLAGS_PTHREADS "$BLACKSCH_SRC" $LDFLAGS -o blackscholes_baseline
$CC $CFLAGS_PTHREADS $VECOPT_FLAGS "$BLACKSCH_SRC" $LDFLAGS -o blackscholes_vecopt

REPS=5
for i in $(seq 1 $REPS); do
  SEC=$(run_time ./blackscholes_baseline 1 "$BLACKSCH_INPUT" "$BLACKSCH_OUTPUT")
  echo "blackscholes,baseline,$i,$SEC" >> "$BLACKSCH_CSV"
done
for i in $(seq 1 $REPS); do
  SEC=$(run_time ./blackscholes_vecopt 1 "$BLACKSCH_INPUT" "$BLACKSCH_OUTPUT")
  echo "blackscholes,vecopt,$i,$SEC" >> "$BLACKSCH_CSV"
done

popd >/dev/null

BLACKSCH_BASELINE_AVG=$(awk -F, '$1=="blackscholes" && $2=="baseline"{s+=$4;n++} END{printf "%.9f", s/n}' "$BLACKSCH_CSV")
BLACKSCH_VECOPT_AVG=$(awk -F, '$1=="blackscholes" && $2=="vecopt"{s+=$4;n++} END{printf "%.9f", s/n}' "$BLACKSCH_CSV")
echo "blackscholes baseline avg: $BLACKSCH_BASELINE_AVG s"
echo "blackscholes vecopt   avg: $BLACKSCH_VECOPT_AVG s"
echo ""


# ======================================================
# ===== Done =====
# ======================================================
echo "All benchmarks finished."
echo "Results saved to:"
echo "  - $QSORT_CSV"
echo "  - $BASICMATH_CSV"
echo "  - $BLACKSCH_CSV"
