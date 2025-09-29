# LLVM-Pass: VecOpt & veclangc

## Overview

This project provides an LLVM pass (**VecOpt**) for automatic if-conversion to improve vectorization, plus a minimal C frontend (**veclangc**) for kernel experiments. It includes scripts for benchmarking and mixing custom kernels with real-world code.

## Quick Start

### 1. Prerequisites

- Linux
- LLVM (recommended: clang-18), CMake, build-essential, git

### 2. Clone and Setup Third-Party Dependencies

```bash
git clone https://github.com/yourname/LLVM-Pass.git
cd LLVM-Pass

# MiBench (for benchmarks)
git clone --depth=1 https://github.com/embecosm/mibench third_party/mibench

# Tiny-AES-c (for AES demo)
git clone --depth=1 https://github.com/kokke/tiny-AES-c third_party/tiny-AES-c

# xxHash (for hash demo)
git clone --depth=1 https://github.com/Cyan4973/xxHash third_party/xxHash
```

### 3. Build veclangc

```bash
cd veclangc
mkdir -p build && cd build
cmake ..
make -j
cd ../..
```

### 4. Build VecOpt LLVM Pass

```bash
mkdir -p build && cd build
cmake ..
make -j
cd ..
```

### 5. Run Benchmarks

Example: Run mixed qsort benchmark and compare outputs

```bash
cd script
bash mix_qsort.sh
```

Other scripts:
- `mix_basicmath.sh` — mixed basicmath benchmark
- `mix_aes.sh` — mixed AES S-box demo
- `run_vector_bench.sh` — full vectorization benchmarks

### 6. Tips

- All third-party code is downloaded into `third_party/`.
- Results and binaries are placed in `build/` and `results/`.

---

**Project Structure**
- `src/` — LLVM Pass (VecOpt)
- `veclangc/` — Minimal C frontend
- `script/` — Benchmark and demo scripts
- `third_party/` — External benchmarks and libraries
