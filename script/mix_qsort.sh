#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIBENCH_DIR="${ROOT_DIR}/third_party/mibench"
BUILD_DIR="${ROOT_DIR}/build/qsort"
RESULTS_DIR="${ROOT_DIR}/results"

QSORT_DIR="${MIBENCH_DIR}/automotive/qsort"
OUT_DIR="${BUILD_DIR}"

VECC="${ROOT_DIR}/build/veclangc/veclangc"
CC="clang"
CFLAGS="-O3 -march=native -ffast-math -fno-exceptions -fno-rtti -std=gnu89"
LDFLAGS="-lm"

mkdir -p "${OUT_DIR}" "${RESULTS_DIR}"

SRC_KERNEL_V="${ROOT_DIR}/veclangc/mix_1/kernel.c"
SRC_KERNEL_H="${ROOT_DIR}/veclangc/mix_1/kernel.h"
SRC_MAIN_REST="${ROOT_DIR}/veclangc/mix_1/qsort_large_rest.c"

# ========== 1) mixed build ==========
echo "[mixed] compiling kernel_qsort.c with veclangc..."
if "${VECC}" --input "${SRC_KERNEL_V}" -c -o "${OUT_DIR}/kernel_qsort.o"; then
  echo "veclangc compiled kernel_qsort.c successfully"
else
  echo "veclangc failed, falling back to clang"
  ${CC} ${CFLAGS} -c "${SRC_KERNEL_V}" -o "${OUT_DIR}/kernel_qsort.o"
fi

echo "[mixed] compiling rest with clang..."
${CC} ${CFLAGS} -c "${SRC_MAIN_REST}" -o "${OUT_DIR}/qsort_large_rest.o"

echo "[mixed] linking..."
${CC} ${CFLAGS} \
  "${OUT_DIR}/qsort_large_rest.o" \
  "${OUT_DIR}/kernel_qsort.o" \
  ${LDFLAGS} -o "${OUT_DIR}/qsort_mixed"

# ========== 2) baseline build ==========
echo "[baseline] building full clang version..."
${CC} ${CFLAGS} \
  "${QSORT_DIR}/qsort_large.c" \
  ${LDFLAGS} -o "${OUT_DIR}/qsort_baseline"

echo
echo "== DONE =="
echo "baseline -> ${OUT_DIR}/qsort_baseline"
echo "mixed    -> ${OUT_DIR}/qsort_mixed"
echo
echo "Try run:"
echo "  ${OUT_DIR}/qsort_baseline ${QSORT_DIR}/input_large.dat | head -n 20"
echo "  ${OUT_DIR}/qsort_mixed    ${QSORT_DIR}/input_large.dat | head -n 20"
