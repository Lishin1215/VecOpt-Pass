#pragma once
#include <memory>
#include <string>
#include <llvm/Target/TargetMachine.h>

namespace llvm { class Module; }

// Create a TargetMachine from a target triple string.
std::unique_ptr<llvm::TargetMachine> createTargetMachineFromTriple(const std::string &triple);

// (Debug only) Build a hard-coded IR for: int sad(const int*, const int*, int)
void buildSADKernelIR(llvm::Module &M);

// Run the standard O3 pipeline using PassBuilder.
void runO3Pipeline(llvm::Module &M);

// Emit an object file (.o) from a module with a given TargetMachine.
void emitObjectFile(llvm::Module &M, llvm::TargetMachine &TM, const std::string &outPath);

// AST -> IR (our tiny C subset)
struct FuncAST;
void buildFromAST(llvm::Module &M, const FuncAST &F);
