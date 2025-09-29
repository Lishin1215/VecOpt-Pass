#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIBENCH_DIR="${ROOT_DIR}/third_party/mibench"
BUILD_DIR="${ROOT_DIR}/build"
RESULTS_DIR="${ROOT_DIR}/results"

BASICMATH_DIR="${MIBENCH_DIR}/automotive/basicmath"
OUT_DIR="${BUILD_DIR}/basicmath"

VECC="${ROOT_DIR}/build/veclangc/veclangc"
# CC=${CC:-clang}
# CFLAGS=${CFLAGS:--O2}
# LDFLAGS=${LDFLAGS:--lm}
CC="clang"
CFLAGS="-O3 -march=native -ffast-math -fno-exceptions -fno-rtti -std=gnu89"
LDFLAGS="-lm"

mkdir -p "${OUT_DIR}" "${RESULTS_DIR}"

SRC_KERNEL_V="${ROOT_DIR}/veclangc/mix_2/kernel_basicmath.c"     
SRC_KERNEL_H="${ROOT_DIR}/veclangc/mix_2/kernel_basicmath.h"
SRC_MAIN_REST="${ROOT_DIR}/veclangc/mix_2/basicmath_large_rest.c"
SRC_CUBIC="${BASICMATH_DIR}/cubic.c"
SRC_RAD2DEG="${BASICMATH_DIR}/rad2deg.c"



# 2) mixed：kernel_basicmath.c → veclangc，其餘 clang（且不連 isqrt.c）
echo "[mixed] compiling kernel_basicmath.c with veclangc..."
"${VECC}" --input "${SRC_KERNEL_V}" -c -o "${OUT_DIR}/kernel_basicmath.o"

echo "[mixed] compiling rest with clang..."
${CC} ${CFLAGS} -c "${SRC_MAIN_REST}" -o "${OUT_DIR}/basicmath_large_rest.o"
${CC} ${CFLAGS} -c "${SRC_CUBIC}" -o "${OUT_DIR}/cubic.o"
${CC} ${CFLAGS} -c "${SRC_RAD2DEG}" -o "${OUT_DIR}/rad2deg.o"

echo "[mixed] linking..."
${CC} ${CFLAGS} \
  "${OUT_DIR}/basicmath_large_rest.o" \
  "${OUT_DIR}/cubic.o" \
  "${OUT_DIR}/rad2deg.o" \
  "${OUT_DIR}/kernel_basicmath.o" \
  ${LDFLAGS} -o "${OUT_DIR}/basicmath_mixed"



  # 1) baseline（完整原始版本，全 clang）
echo "[baseline] building..."
${CC} ${CFLAGS} \
  "${BASICMATH_DIR}/basicmath_large.c" \
  "${BASICMATH_DIR}/cubic.c" \
  "${BASICMATH_DIR}/isqrt.c" \
  "${BASICMATH_DIR}/rad2deg.c" \
  ${LDFLAGS} -o "${OUT_DIR}/basicmath_baseline"

echo
echo "== DONE =="
echo "baseline -> ${OUT_DIR}/basicmath_baseline"
echo "mixed    -> ${OUT_DIR}/basicmath_mixed"
echo
echo "Try run:"
echo "  ${OUT_DIR}/basicmath_baseline  | head -n 20"
echo "  ${OUT_DIR}/basicmath_mixed     | head -n 20"
