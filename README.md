# VecOpt: A Simple Vectorization Optimization Pass

This is a project to build an LLVM pass that finds and optimizes common code patterns that are unfriendly to CPU vectorization.ddddd

---
## What is this?

Modern CPUs are like high-speed factory pipelines; they love processing linear, uninterrupted tasks. However, an `if-else` statement in code is like a fork in the pipeline. The CPU has to guess which path to take. If it guesses wrong (a "branch misprediction"), the entire pipeline has to be stopped and restarted, causing a significant performance penalty. This is especially costly inside loops.

`VecOpt`'s job is to find these `if-else` forks inside loops and "straighten them out" by converting them into linear, branch-free instructions that CPUs prefer. This process is called **If-Conversion**.

---
## Current Progress

The project is currently a functional prototype that can successfully analyze and transform a test case.

- [x] **Core Feature: If-Conversion Pass**
  - Successfully implemented an LLVM `FunctionPass` to analyze code within a function.
  - Can accurately identify the "diamond" control-flow pattern created by `if-else` statements.

- [x] **Diagnose Mode (`--diagnose`)**
  - Scans LLVM IR and prints a report identifying potential optimization sites for If-Conversion without modifying the code.

- [x] **Rewrite Mode (`--rewrite`)**
  - Safely and automatically transforms identified diamond patterns from `br` and `phi` instructions into a more efficient `select` instruction.

- [x] **Automated Test Script (`run_all.sh`)**
  - Provides a one-command shell script that automates the entire process of building the project, generating test IR, and running the pass, greatly simplifying the development cycle.

---
## Work in Progress

With the foundation in place, the next goal is to expand the pass's capabilities to make it smarter and more powerful.

- [ ] **Expand Diagnostic Capabilities**
  - In addition to If-Conversion, teach `VecOpt` to detect other common issues that prevent vectorization, such as **function calls inside loops**.

- [ ] **Enhance Rewrite Capabilities**
  - Handle more complex If-Conversion scenarios, such as a single diamond pattern that produces multiple results (corresponding to multiple `phi` nodes).

- [ ] **Improve the Toolchain**
  - Make the `run_all.sh` script more flexible, for example, by allowing it to accept different C files as input.

---
## How to Use

1.  **Requirements**:
    - `clang-18`
    - `llvm-18`
    - `cmake`

2.  **Execution**:
    From the project's root directory, simply run the automation script:
    ```bash
    ./run_all.sh
    ```

3.  **Expected Output**:
    - The script will build the project and generate all artifacts in the `build/` directory.
    - You will see the output from both **Diagnose Mode** and **Rewrite Mode** in the terminal.
    - Finally, a successfully optimized LLVM IR file named `sad.rewritten.ll` will be created in the `build/` directory.