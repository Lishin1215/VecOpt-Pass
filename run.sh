#!/bin/bash

# --- Automated build and run script for the VecOpt Pass ---
# This version uses absolute paths to be runnable from anywhere.

# Exit immediately if any command fails
set -e

# --- Configuration ---
# Get the absolute path of the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Define paths relative to the script's location
SRC_FILE="$SCRIPT_DIR/benchmarks/sad.c"
BUILD_DIR="$SCRIPT_DIR/build"
PASS_PLUGIN_SO="$BUILD_DIR/VecOpt.so"

# --- 1. Rebuild the project ---
echo "--- (1/5) Rebuilding the project... ---"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake "$SCRIPT_DIR"
make -j$(nproc)
echo "--- Build complete ---"
echo ""

# --- 2. Generate LLVM IR from C source ---
echo "--- (2/5) Generating LLVM IR from C source... ---"
clang-18 -O0 -g -emit-llvm -S "$SRC_FILE" -o sad.O0.ll
echo "Successfully generated sad.O0.ll"
echo ""

# --- 3. Run mem2reg Pass ---
echo "--- (3/5) Running mem2reg Pass... ---"
opt-18 -passes=mem2reg -S sad.O0.ll -o sad.mem2reg.ll
echo "Successfully generated sad.mem2reg.ll"
echo ""

# --- 4. Remove 'optnone' attribute ---
echo "--- (4/5) Removing 'optnone' attribute from IR... ---"
sed -i 's/ optnone//g' sad.mem2reg.ll
echo "'optnone' attribute removed successfully"
echo ""

# --- 5. Run the VecOpt Pass ---
echo "--- (5/5) Running the VecOpt Pass... ---"

# Diagnose Mode
echo "--- [Diagnose Mode] ---"
opt-18 -load-pass-plugin="$PASS_PLUGIN_SO" -passes=vecopt -disable-output sad.mem2reg.ll

echo ""

# Rewrite Mode
echo "--- [Rewrite Mode] ---"
opt-18 -load-pass-plugin="$PASS_PLUGIN_SO" -passes=vecopt -vecopt-rewrite sad.mem2reg.ll -S -o sad.rewritten.ll
echo "Rewrite complete! Result saved to sad.rewritten.ll"
echo ""

echo "--- All steps completed successfully! ---"