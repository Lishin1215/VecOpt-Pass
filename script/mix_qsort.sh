#!/usr/bin/env bash
set -euo pipefail

# 1. 混合編譯
../veclangc/build/veclangc --input ../third_party/mibench/automotive/qsort/kernel.c -c -o ../third_party/mibench/automotive/qsort/kernel.o || {
  echo "veclangc 編譯失敗，使用 clang fallback"
  clang -c ../third_party/mibench/automotive/qsort/kernel.c -o ../third_party/mibench/automotive/qsort/kernel.o
}
clang -c ../third_party/mibench/automotive/qsort/qsort_large_rest.c -o ../third_party/mibench/automotive/qsort/rest.o
clang ../third_party/mibench/automotive/qsort/rest.o ../third_party/mibench/automotive/qsort/kernel.o -lm -o ../third_party/mibench/automotive/qsort/qsort_mixed

# 2. 純 clang 編譯
clang ../third_party/mibench/automotive/qsort/qsort_large.c -lm -o ../third_party/mibench/automotive/qsort/qsort_clang

INPUT_FILE="../third_party/mibench/automotive/qsort/input_min.dat"
EXEC_MIXED="../third_party/mibench/automotive/qsort/qsort_mixed"
EXEC_CLANG="../third_party/mibench/automotive/qsort/qsort_clang"

# TMP_MIXED=$(mktemp)
# TMP_CLANG=$(mktemp)

# echo "TMP_MIXED: $TMP_MIXED"
# echo "TMP_CLANG: $TMP_CLANG"

if [ -f "$INPUT_FILE" ]; then
    # "$EXEC_MIXED" "$INPUT_FILE" > "$TMP_MIXED"
    # "$EXEC_CLANG" "$INPUT_FILE" > "$TMP_CLANG"
    "$EXEC_MIXED" "$INPUT_FILE" 
    "$EXEC_CLANG" "$INPUT_FILE"
    # echo "執行完畢，暫存檔案如下："
    # echo "TMP_MIXED: $TMP_MIXED"
    # echo "TMP_CLANG: $TMP_CLANG"
    # if diff -q "$TMP_MIXED" "$TMP_CLANG" >/dev/null; then
    #     echo "輸出結果一致"
    # else
    #     echo "輸出結果不同"
    # fi
    # rm -f "$TMP_MIXED" "$TMP_CLANG"
else
    echo "錯誤: 找不到輸入檔案 $INPUT_FILE"
fi