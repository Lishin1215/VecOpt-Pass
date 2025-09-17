#!/usr/bin/env bash
set -euo pipefail

# Set output directory to relative path
OUT_DIR="../veclangc/mix_1"
mkdir -p "$OUT_DIR"

# 1. Mixed compilation: try to compile kernel with veclangc, fallback to clang if it fails
../veclangc/build/veclangc --input ../third_party/mibench/automotive/qsort/kernel.c -c -o "$OUT_DIR/kernel.o" || {
  echo "veclangc compilation failed, using clang fallback"
  clang -c ../third_party/mibench/automotive/qsort/kernel.c -o "$OUT_DIR/kernel.o"
}

# Compile the rest of qsort with clang
clang -c ../third_party/mibench/automotive/qsort/qsort_large_rest.c -o "$OUT_DIR/rest.o"

# Link the mixed object files to generate the mixed executable
clang "$OUT_DIR/rest.o" "$OUT_DIR/kernel.o" -lm -o "$OUT_DIR/qsort_mixed"

# 2. Pure clang compilation
clang ../third_party/mibench/automotive/qsort/qsort_large.c -lm -o "$OUT_DIR/qsort_clang"

# Allow input file override via argument or environment variable
INPUT_FILE="${1:-${INPUT_FILE:-../third_party/mibench/automotive/qsort/input_min.dat}}"
EXEC_MIXED="$OUT_DIR/qsort_mixed"
EXEC_CLANG="$OUT_DIR/qsort_clang"

TMP_MIXED=$(mktemp)
TMP_CLANG=$(mktemp)
trap 'rm -f "$TMP_MIXED" "$TMP_CLANG"' EXIT

if [ -f "$INPUT_FILE" ]; then
    echo "Running: $EXEC_MIXED $INPUT_FILE"
    "$EXEC_MIXED" "$INPUT_FILE" > "$TMP_MIXED"
    echo "Running: $EXEC_CLANG $INPUT_FILE"
    "$EXEC_CLANG" "$INPUT_FILE" > "$TMP_CLANG"
    echo "=== Output diff ==="
    if diff -u "$TMP_MIXED" "$TMP_CLANG"; then
        echo "Outputs are identical"
    else
        echo "Outputs differ"
        exit 1
    fi
else
    echo "Error: Input file not found: $INPUT_FILE"
    exit 2
fi