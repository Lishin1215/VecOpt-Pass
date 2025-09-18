#!/usr/bin/env bash
set -euo pipefail

# ---- Path settings (according to your project structure) ----
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
THIRD_PARTY="${REPO_ROOT}/third_party"
MIX_DIR="${REPO_ROOT}/veclangc/mix_3"
VECC="${REPO_ROOT}/build/veclangc/veclangc"

TINY_DIR="${THIRD_PARTY}/tiny-AES-c"

mkdir -p "${THIRD_PARTY}" "${MIX_DIR}"

# ---- 1) Get upstream tiny-AES-c ----
if [ ! -d "${TINY_DIR}/.git" ]; then
  echo "[clone] github.com/kokke/tiny-AES-c -> ${TINY_DIR}"
  git clone --depth=1 https://github.com/kokke/tiny-AES-c "${TINY_DIR}"
else
  echo "[update] tiny-AES-c"
  git -C "${TINY_DIR}" fetch -q origin
  git -C "${TINY_DIR}" reset -q --hard origin/master
fi

# ---- 2) Build tiny-AES baseline (pure clang) ----
echo "[clang] build tiny-AES baseline (aes_clang)"
clang -O3 -c "${TINY_DIR}/aes.c" -o "${MIX_DIR}/aes.o"
clang -O3 "${TINY_DIR}/test.c" "${MIX_DIR}/aes.o" -o "${MIX_DIR}/aes_clang"

# ---- 3) Generate S-box lookup kernel for veclangc (all int + while + []) ----
KERNEL_C="${MIX_DIR}/kernel_aes.c"
DRIVER_C="${MIX_DIR}/rest_aes.c"


# ---- 4) Compile subbytes kernel (try veclangc first, fallback to clang) ----
echo "[mixed] build subbytes: kernel with veclangc (fallback clang)"
set +e
"${VECC}" --input "${KERNEL_C}" -c -o "${MIX_DIR}/kernel_aes.o"
VECC_RC=$?
set -e
if [ $VECC_RC -ne 0 ]; then
  echo "[warn] veclangc failed to compile kernel, fallback to clang (still runnable but no vectorization demo)"
  clang -O3 -c "${KERNEL_C}" -o "${MIX_DIR}/kernel_aes.o"
fi

# ---- 5) Link subbytes test program (mixed and pure clang use the same driver) ----
echo "[link] subbytes_mixed"
clang -O3 "${DRIVER_C}" "${MIX_DIR}/kernel_aes.o" -o "${MIX_DIR}/subbytes_mixed"

# ---- 6) Tips and run ----
echo
echo "==== build done ===="
echo "AES baseline (pure clang): ${MIX_DIR}/aes_clang"
echo "SubBytes demo (mixed):   ${MIX_DIR}/subbytes_mixed  [default processes 16MB, can add argument to change size]"
echo
echo "Reminder: Your veclangc currently does not support XOR/shift/byte, so the AES core remains clang for now."
echo "Once you add bitwise operations and uint8_t, you can rewrite the AES SubBytes/ShiftRows/AddRoundKey inner loop as kernel."
echo

# Optional: quick run
"${MIX_DIR}/aes_clang" >/dev/null 2>&1 || true
"${MIX_DIR}/subbytes_mixed" $((1<<22))  # 4MB example