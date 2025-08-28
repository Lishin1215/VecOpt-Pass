#pragma once
#include <memory>
#include <string>
namespace llvm {
class Module;
class TargetMachine;
}

std::unique_ptr<llvm::TargetMachine> createTargetMachineFromTriple(const std::string &triple);
void buildSADKernelIR(llvm::Module &M); // generate：int sad(const int*, const int*, int)
void runO3Pipeline(llvm::Module &M);
void emitObjectFile(llvm::Module &M, llvm::TargetMachine &TM, const std::string &outPath);
