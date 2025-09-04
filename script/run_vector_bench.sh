#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RESULTS_DIR="$ROOT/results"
BUILD_DIR="$ROOT/build/bench"
MIBENCH_DIR="$ROOT/third_party/mibench"

# ===== prep =====
mkdir -p "$RESULTS_DIR" "$BUILD_DIR"

# Check for MiBench repo
if [ ! -d "$MIBENCH_DIR/.git" ]; then
  mkdir -p "$(dirname "$MIBENCH_DIR")"
  git clone --depth=1 https://github.com/embecosm/mibench "$MIBENCH_DIR"
fi

# Confirm VecOpt.so exists
VECOPT_SO="$ROOT/build/VecOpt.so"
if [ ! -f "$VECOPT_SO" ]; then
  echo "ERROR: VecOpt.so not found at $VECOPT_SO"
  echo "Please build VecOpt.so in $ROOT/build first using CMake/Make."
  exit 1
fi

# Common compiler flags
CC="clang-18"
CFLAGS="-O3 -march=native -ffast-math -fno-exceptions -fno-rtti -std=gnu89"
LDFLAGS="-lm"

# VecOpt pass flags
VECOPT_FLAGS="-fpass-plugin=$VECOPT_SO -Rpass=loop-vectorize -Rpass=slp-vectorizer"

# ======================================================
# # ===== benchmark: dijkstra (network/dijkstra) =====
# # ======================================================
# echo "== MiBench: network/dijkstra =="

# DIJ_DIR="$MIBENCH_DIR/network/dijkstra"
# DIJ_CSV="$RESULTS_DIR/dijkstra_times.csv"

# mkdir -p "$BUILD_DIR/dijkstra"
# pushd "$BUILD_DIR/dijkstra" >/dev/null

# # Prepare CSV file for dijkstra
# : > "$DIJ_CSV"
# echo "benchmark,variant,run_idx,seconds" >> "$DIJ_CSV"

# # Compile both variants
# $CC $CFLAGS "$DIJ_DIR/dijkstra_large.c" -o dijkstra_baseline
# $CC $CFLAGS $VECOPT_FLAGS "$DIJ_DIR/dijkstra_large.c" -o dijkstra_vecopt

# DIJ_INPUT="$DIJ_DIR/input.dat"

# # Run baseline
# REPS=50
# for i in $(seq 1 $REPS); do
#   SEC=$({ /usr/bin/time -f "%e" ./dijkstra_baseline "$DIJ_INPUT" >/dev/null; } 2>&1)
#   echo "dijkstra,baseline,$i,$SEC" >> "$DIJ_CSV"
# done

# # Run vecopt
# for i in $(seq 1 $REPS); do
#   SEC=$({ /usr/bin/time -f "%e" ./dijkstra_vecopt "$DIJ_INPUT" >/dev/null; } 2>&1)
#   echo "dijkstra,vecopt,$i,$SEC" >> "$DIJ_CSV"
# done

# popd >/dev/null

# # --- Calculate and print dijkstra stats ---
# BASELINE_SUM=$(awk -F, '$1=="dijkstra" && $2=="baseline"{s+=$4} END{print s}' "$DIJ_CSV")
# BASELINE_AVG=$(awk -F, '$1=="dijkstra" && $2=="baseline"{s+=$4;n++} END{if(n>0) printf "%.4f", s/n}' "$DIJ_CSV")
# VECOPT_SUM=$(awk -F, '$1=="dijkstra" && $2=="vecopt"{s+=$4} END{print s}' "$DIJ_CSV")
# VECOPT_AVG=$(awk -F, '$1=="dijkstra" && $2=="vecopt"{s+=$4;n++} END{if(n>0) printf "%.4f", s/n}' "$DIJ_CSV")

# echo "dijkstra baseline total: $BASELINE_SUM s, avg: $BASELINE_AVG s"
# echo "dijkstra vecopt   total: $VECOPT_SUM s, avg: $VECOPT_AVG s"
# echo ""


# ===== MiBench: qsort =====
echo "== MiBench: automotive/qsort =="

QSORT_DIR="$MIBENCH_DIR/automotive/qsort"
mkdir -p "$BUILD_DIR/qsort"
pushd "$BUILD_DIR/qsort" >/dev/null

$CC $CFLAGS "$QSORT_DIR/qsort_large.c" $LDFLAGS -o qsort_baseline
$CC $CFLAGS $VECOPT_FLAGS "$QSORT_DIR/qsort_large.c" $LDFLAGS -o qsort_vecopt

QSORT_INPUT="$QSORT_DIR/input_5m.dat"

REPS=20
for i in $(seq 1 $REPS); do
  SEC=$(/usr/bin/time -f "%e" ./qsort_baseline "$QSORT_INPUT" 2>&1 1>/dev/null | tail -n1)
  if [[ "$SEC" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
    echo "qsort,baseline,$i,$SEC" >> "$CSV"
  fi
done

for i in $(seq 1 $REPS); do
  SEC=$(/usr/bin/time -f "%e" ./qsort_vecopt "$QSORT_INPUT" 2>&1 1>/dev/null | tail -n1)
  if [[ "$SEC" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
    echo "qsort,vecopt,$i,$SEC" >> "$CSV"
  fi
done

popd >/dev/null


# ======================================================
# ===== benchmark: basicmath_large =====================
# ======================================================
echo "== MiBench: basicmath_large =="

BASICMATH_DIR="$MIBENCH_DIR/automotive/basicmath"
BASICMATH_CSV="$RESULTS_DIR/basicmath_times.csv"

mkdir -p "$BUILD_DIR/basicmath"
pushd "$BUILD_DIR/basicmath" >/dev/null

# Prepare CSV file for basicmath
: > "$BASICMATH_CSV"
echo "benchmark,variant,run_idx,seconds" >> "$BASICMATH_CSV"

# Compile both variants
$CC $CFLAGS "$BASICMATH_DIR/basicmath_large.c" "$BASICMATH_DIR/cubic.c" "$BASICMATH_DIR/isqrt.c" "$BASICMATH_DIR/rad2deg.c" $LDFLAGS -o basicmath_baseline
$CC $CFLAGS $VECOPT_FLAGS "$BASICMATH_DIR/basicmath_large.c" "$BASICMATH_DIR/cubic.c" "$BASICMATH_DIR/isqrt.c" "$BASICMATH_DIR/rad2deg.c" $LDFLAGS -o basicmath_vecopt

# Run baseline
REPS=50
for i in $(seq 1 $REPS); do
  SEC=$({ /usr/bin/time -f "%e" ./basicmath_baseline 1>/dev/null; } 2>&1)
  echo "basicmath,baseline,$i,$SEC" >> "$BASICMATH_CSV"
done

# Run vecopt
for i in $(seq 1 $REPS); do
  SEC=$({ /usr/bin/time -f "%e" ./basicmath_vecopt 1>/dev/null; } 2>&1)
  echo "basicmath,vecopt,$i,$SEC" >> "$BASICMATH_CSV"
done

popd >/dev/null

# --- Calculate and print basicmath stats ---
BASELINE_SUM=$(awk -F, '$1=="basicmath" && $2=="baseline"{s+=$4} END{print s}' "$BASICMATH_CSV")
BASELINE_AVG=$(awk -F, '$1=="basicmath" && $2=="baseline"{s+=$4;n++} END{if(n>0) printf "%.4f", s/n}' "$BASICMATH_CSV")
VECOPT_SUM=$(awk -F, '$1=="basicmath" && $2=="vecopt"{s+=$4} END{print s}' "$BASICMATH_CSV")
VECOPT_AVG=$(awk -F, '$1=="basicmath" && $2=="vecopt"{s+=$4;n++} END{if(n>0) printf "%.4f", s/n}' "$BASICMATH_CSV")

echo "basicmath baseline total: $BASELINE_SUM s, avg: $BASELINE_AVG s"
echo "basicmath vecopt   total: $VECOPT_SUM s, avg: $VECOPT_AVG s"
echo ""

echo "All benchmarks finished."
echo "Results saved to:"
echo "  - $DIJ_CSV"
echo "  - $BASICMATH_CSV"