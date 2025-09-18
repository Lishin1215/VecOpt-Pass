#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RESULTS_DIR="$ROOT/results"
BUILD_DIR="$ROOT/build/bench"
MIBENCH_DIR="$ROOT/third_party/mibench"
CSV="$RESULTS_DIR/benchmark_times.csv"

# ===== helper =====
run_many () { # cmd reps -> prints times (one per line)
  local cmd="$1"; local reps="${2:-10}"
  for i in $(seq 1 "$reps"); do
    /usr/bin/time -f "%e" bash -c "$cmd" >/dev/null 2>>"$CSV.tmp"
  done
}

# ===== prep =====
mkdir -p "$RESULTS_DIR" "$BUILD_DIR"
: > "$CSV"     # truncate CSV
echo "benchmark,variant,run_idx,seconds" >> "$CSV"

if [ ! -d "$MIBENCH_DIR/.git" ]; then
  mkdir -p "$(dirname "$MIBENCH_DIR")"
  git clone --depth=1 https://github.com/embecosm/mibench "$MIBENCH_DIR"
fi

# confirm VecOpt
VECOPT_SO="$ROOT/build/VecOpt.so"
if [ ! -f "$VECOPT_SO" ]; then
  echo "ERROR: VecOpt.so not found at $VECOPT_SO"
  echo "請先在 $ROOT/build 下用 CMake/Make 產出 VecOpt.so"
  exit 1
fi

# 通用編譯旗標
CC="clang"
CFLAGS="-O3 -march=native -ffast-math -fno-exceptions -fno-rtti -std=gnu89"
LDFLAGS="-lm"

# VecOpt 旗標（Clang 在前端載入你的 Pass 插件）
VECOPT_FLAGS="-fpass-plugin=$VECOPT_SO -Rpass=loop-vectorize -Rpass=slp-vectorizer"

# # ===== benchmark: susan =====
# echo "== MiBench: automotive/susan =="

# SUSAN_SRC="$MIBENCH_DIR/automotive/susan/susan.c"
# SUSAN_IN="$MIBENCH_DIR/automotive/susan/input_large.pgm"
# # 有些倉庫沒有 input_large，用 input.pgm 也行
# if [ ! -f "$SUSAN_IN" ]; then
#   SUSAN_IN="$MIBENCH_DIR/automotive/susan/input.pgm"
# fi

# mkdir -p "$BUILD_DIR/susan"
# pushd "$BUILD_DIR/susan" >/dev/null

# # baseline
# $CC $CFLAGS "$SUSAN_SRC" $LDFLAGS -o susan_baseline

# # vecopt
# $CC $CFLAGS $VECOPT_FLAGS "$SUSAN_SRC" $LDFLAGS -o susan_vecopt

# : > "$CSV.tmp"
# REPS=500  # 建議多一點
# for i in $(seq 1 $REPS); do
#   /usr/bin/time -f "%e" ./susan_baseline "$SUSAN_IN" /dev/null -e >/dev/null 2>>"$CSV.tmp"
#   echo "susan,baseline,$i,$(tail -n1 "$CSV.tmp")" >> "$CSV"
# done
# BASELINE_SUM=$(awk '{s+=$1} END{print s}' "$CSV.tmp")
# BASELINE_AVG=$(awk -v n=$REPS '{s+=$1} END{printf "%.4f", s/n}' "$CSV.tmp")
# echo "susan baseline total: $BASELINE_SUM s, avg: $BASELINE_AVG s"

# : > "$CSV.tmp"
# for i in $(seq 1 $REPS); do
#   /usr/bin/time -f "%e" ./susan_vecopt "$SUSAN_IN" /dev/null -e >/dev/null 2>>"$CSV.tmp"
#   echo "susan,vecopt,$i,$(tail -n1 "$CSV.tmp")" >> "$CSV"
# done
# VECOPT_SUM=$(awk '{s+=$1} END{print s}' "$CSV.tmp")
# VECOPT_AVG=$(awk -v n=$REPS '{s+=$1} END{printf "%.4f", s/n}' "$CSV.tmp")
# echo "susan vecopt total: $VECOPT_SUM s, avg: $VECOPT_AVG s"

# popd >/dev/null

# ===== benchmark: dijkstra (network/dijkstra) =====
# echo "== MiBench: network/dijkstra =="

# DIJ_DIR="$MIBENCH_DIR/network/dijkstra"
# # 官方原始碼：dijkstra_large.c / dijkstra_small.c + Makefile
# # 我們直接編 large 版
# mkdir -p "$BUILD_DIR/dijkstra"
# pushd "$BUILD_DIR/dijkstra" >/dev/null

# $CC $CFLAGS "$DIJ_DIR/dijkstra_large.c" -o dijkstra_baseline
# $CC $CFLAGS $VECOPT_FLAGS "$DIJ_DIR/dijkstra_large.c" -o dijkstra_vecopt

# DIJ_INPUT="$DIJ_DIR/input.dat"
# DIJ_OUTPUT="output_large.dat"

# : > "$CSV.tmp"
# REPS=300
# for i in $(seq 1 $REPS); do
#   /usr/bin/time -f "%e" ./dijkstra_baseline "$DIJ_INPUT" "$DIJ_OUTPUT" >/dev/null 2>>"$CSV.tmp"
#   LAST=$(tail -n1 "$CSV.tmp")
#   if [[ "$LAST" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
#     echo "dijkstra,baseline,$i,$LAST" >> "$CSV"
#   fi
# done

# : > "$CSV.tmp"
# for i in $(seq 1 $REPS); do
#   /usr/bin/time -f "%e" ./dijkstra_vecopt "$DIJ_INPUT" "$DIJ_OUTPUT" >/dev/null 2>>"$CSV.tmp"
#   LAST=$(tail -n1 "$CSV.tmp")
#   if [[ "$LAST" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
#     echo "dijkstra,vecopt,$i,$LAST" >> "$CSV"
#   fi
# done
# popd >/dev/null

# rm -f "$CSV.tmp"
# echo "Results CSV: $CSV"

# # ===== 統計總時間與平均時間 =====
# BASELINE_SUM=$(awk -F, '$2=="baseline"{s+=$4} END{print s}' "$CSV")
# BASELINE_AVG=$(awk -F, '$2=="baseline"{s+=$4;n++} END{if(n>0) printf "%.4f", s/n}' "$CSV")
# VECOPT_SUM=$(awk -F, '$2=="vecopt"{s+=$4} END{print s}' "$CSV")
# VECOPT_AVG=$(awk -F, '$2=="vecopt"{s+=$4;n++} END{if(n>0) printf "%.4f", s/n}' "$CSV")

# echo "dijkstra baseline total: $BASELINE_SUM s, avg: $BASELINE_AVG s"
# echo "dijkstra vecopt   total: $VECOPT_SUM s, avg: $VECOPT_AVG s"





echo "== PARSEC: blackscholes =="

PARSEC_DIR="$ROOT/third_party/parsec-benchmark"
BLACKSCH_SRC="$PARSEC_DIR/pkgs/apps/blackscholes/src/blackscholes.c"
BLACKSCH_INPUT="$PARSEC_DIR/pkgs/apps/blackscholes/inputs/in_1m.txt"
BLACKSCH_OUTPUT="out.txt"

mkdir -p "$BUILD_DIR/blackscholes"
pushd "$BUILD_DIR/blackscholes" >/dev/null
CFLAGS_PTHREADS="$CFLAGS -pthread"

$CC $CFLAGS_PTHREADS "$BLACKSCH_SRC" $LDFLAGS -o blackscholes_baseline
$CC $CFLAGS_PTHREADS $VECOPT_FLAGS "$BLACKSCH_SRC" $LDFLAGS -o blackscholes_vecopt
REPS=5
for i in $(seq 1 $REPS); do
  SEC=$(/usr/bin/time -f "%e" ./blackscholes_baseline 1 "$BLACKSCH_INPUT" "$BLACKSCH_OUTPUT" 2>&1 1>/dev/null | tail -n1)
  if [[ "$SEC" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
    echo "blackscholes,baseline,$i,$SEC" >> "$CSV"
  fi
done

for i in $(seq 1 $REPS); do
  SEC=$(/usr/bin/time -f "%e" ./blackscholes_vecopt 1 "$BLACKSCH_INPUT" "$BLACKSCH_OUTPUT" 2>&1 1>/dev/null | tail -n1)
  if [[ "$SEC" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
    echo "blackscholes,vecopt,$i,$SEC" >> "$CSV"
  fi
done
popd >/dev/null



# echo "== PARSEC: swaptions =="

# CXX="clang++-18"
# CXXFLAGS="-O3 -march=native -ffast-math -fno-exceptions -fno-rtti -std=c++03"
# LDFLAGS="-lm"

# SWAP_SRC="$PARSEC_DIR/pkgs/apps/swaptions/src"
# SWAP_INPUT="-ns 1000 -sm 10000 -nt 1"
# mkdir -p "$BUILD_DIR/swaptions"
# pushd "$BUILD_DIR/swaptions" >/dev/null

# $CXX $CXXFLAGS $VECOPT_FLAGS $SWAP_SRC/*.cpp $SWAP_SRC/*.c $LDFLAGS -o swaptions_vecopt
# $CXX $CXXFLAGS $SWAP_SRC/*.cpp $SWAP_SRC/*.c $LDFLAGS -o swaptions_baseline

# REPS=20
# for i in $(seq 1 $REPS); do
#   SEC=$(/usr/bin/time -f "%e" ./swaptions_baseline $SWAP_INPUT 2>&1 1>/dev/null | tail -n1)
#   if [[ "$SEC" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
#     echo "swaptions,baseline,$i,$SEC" >> "$CSV"
#   fi
# done

# for i in $(seq 1 $REPS); do
#   SEC=$(/usr/bin/time -f "%e" ./swaptions_vecopt $SWAP_INPUT 2>&1 1>/dev/null | tail -n1)
#   if [[ "$SEC" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
#     echo "swaptions,vecopt,$i,$SEC" >> "$CSV"
#   fi
# done
# popd >/dev/null

# echo "== MiBench: basicmath_large =="

# BASICMATH_DIR="$MIBENCH_DIR/automotive/basicmath"
# mkdir -p "$BUILD_DIR/basicmath"
# pushd "$BUILD_DIR/basicmath" >/dev/null

# $CC $CFLAGS "$BASICMATH_DIR/basicmath_large.c" "$BASICMATH_DIR/cubic.c" "$BASICMATH_DIR/isqrt.c" "$BASICMATH_DIR/rad2deg.c" $LDFLAGS -o basicmath_baseline
# $CC $CFLAGS $VECOPT_FLAGS "$BASICMATH_DIR/basicmath_large.c" "$BASICMATH_DIR/cubic.c" "$BASICMATH_DIR/isqrt.c" "$BASICMATH_DIR/rad2deg.c" $LDFLAGS -o basicmath_vecopt

# REPS=50
# for i in $(seq 1 $REPS); do
#   SEC=$({ /usr/bin/time -f "%e" ./basicmath_baseline 1>/dev/null; } 2>&1)
#   echo "basicmath,baseline,$i,$SEC" >> "$CSV"
# done

# for i in $(seq 1 $REPS); do
#   SEC=$({ /usr/bin/time -f "%e" ./basicmath_vecopt 1>/dev/null; } 2>&1)
#   echo "basicmath,vecopt,$i,$SEC" >> "$CSV"
# done
# popd >/dev/null

# BASELINE_SUM=$(awk -F, '$1=="basicmath" && $2=="baseline"{s+=$4} END{print s}' "$CSV")
# BASELINE_AVG=$(awk -F, '$1=="basicmath" && $2=="baseline"{s+=$4;n++} END{if(n>0) printf "%.4f", s/n}' "$CSV")
# VECOPT_SUM=$(awk -F, '$1=="basicmath" && $2=="vecopt"{s+=$4} END{print s}' "$CSV")
# VECOPT_AVG=$(awk -F, '$1=="basicmath" && $2=="vecopt"{s+=$4;n++} END{if(n>0) printf "%.4f", s/n}' "$CSV")

# echo "basicmath baseline total: $BASELINE_SUM s, avg: $BASELINE_AVG s"
# echo "basicmath vecopt   total: $VECOPT_SUM s, avg: $VECOPT_AVG s"


# ===== MiBench: qsort =====
# echo "== MiBench: automotive/qsort =="

# QSORT_DIR="$MIBENCH_DIR/automotive/qsort"
# mkdir -p "$BUILD_DIR/qsort"
# pushd "$BUILD_DIR/qsort" >/dev/null

# $CC $CFLAGS "$QSORT_DIR/qsort_large.c" $LDFLAGS -o qsort_baseline
# $CC $CFLAGS $VECOPT_FLAGS "$QSORT_DIR/qsort_large.c" $LDFLAGS -o qsort_vecopt

# QSORT_INPUT="$QSORT_DIR/input_5m.dat"

# REPS=20
# for i in $(seq 1 $REPS); do
#   SEC=$(/usr/bin/time -f "%e" ./qsort_baseline "$QSORT_INPUT" 2>&1 1>/dev/null | tail -n1)
#   if [[ "$SEC" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
#     echo "qsort,baseline,$i,$SEC" >> "$CSV"
#   fi
# done

# for i in $(seq 1 $REPS); do
#   SEC=$(/usr/bin/time -f "%e" ./qsort_vecopt "$QSORT_INPUT" 2>&1 1>/dev/null | tail -n1)
#   if [[ "$SEC" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
#     echo "qsort,vecopt,$i,$SEC" >> "$CSV"
#   fi
# done

# popd >/dev/null