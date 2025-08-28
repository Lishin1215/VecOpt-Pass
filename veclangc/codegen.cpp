#include "codegen.h"
#include <llvm/ADT/Triple.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/CodeGen/CommandFlags.h>
#include <llvm/IR/LegacyPassManager.h>

using namespace llvm;

std::unique_ptr<TargetMachine> createTargetMachineFromTriple(const std::string &tripleStr) {
  std::string Error;
  const Target *T = TargetRegistry::lookupTarget(tripleStr, Error);
  if (!T) {
    errs() << "lookupTarget failed: " << Error << "\n";
    return nullptr;
  }
  std::string CPU = sys::getHostCPUName().str();
  std::string Features;
  TargetOptions opt;
  auto RM = std::optional<Reloc::Model>();
  std::unique_ptr<TargetMachine> TM(
      T->createTargetMachine(tripleStr, CPU, Features, opt, RM));
  return TM;
}

void buildSADKernelIR(Module &M) {
  LLVMContext &C = M.getContext();
  Type *i32 = Type::getInt32Ty(C);
  Type *pi32 = PointerType::getUnqual(i32);

  FunctionType *FT = FunctionType::get(i32, {pi32, pi32, i32}, false);
  Function *F = Function::Create(FT, Function::ExternalLinkage, "sad", M);

  
  auto AI = F->arg_begin();
  Value *A = &*AI++; A->setName("a");
  Value *B = &*AI++; B->setName("b");
  Value *N = &*AI++; N->setName("n");

  BasicBlock *Entry = BasicBlock::Create(C, "entry", F);
  IRBuilder<> Bld(Entry);

  
  Value *zero = ConstantInt::get(i32, 0);
  AllocaInst *iAlloca   = Bld.CreateAlloca(i32, nullptr, "i");
  AllocaInst *sumAlloca = Bld.CreateAlloca(i32, nullptr, "sum");
  Bld.CreateStore(zero, iAlloca);
  Bld.CreateStore(zero, sumAlloca);

  BasicBlock *LoopCond = BasicBlock::Create(C, "loop.cond", F);
  BasicBlock *LoopBody = BasicBlock::Create(C, "loop.body", F);
  BasicBlock *LoopLatch= BasicBlock::Create(C, "loop.latch",F);
  BasicBlock *Exit     = BasicBlock::Create(C, "exit",      F);
  Bld.CreateBr(LoopCond);

  // cond: i < n ?
  Bld.SetInsertPoint(LoopCond);
  Value *iVal = Bld.CreateLoad(i32, iAlloca, "i.val");
  Value *cmp  = Bld.CreateICmpSLT(iVal, N, "cmp");
  Bld.CreateCondBr(cmp, LoopBody, Exit);

  // body:
  Bld.SetInsertPoint(LoopBody);
  // a[i], b[i]
  Value *aPtr = Bld.CreateInBoundsGEP(i32, A, iVal, "a.idx");
  Value *bPtr = Bld.CreateInBoundsGEP(i32, B, iVal, "b.idx");
  Value *aVal = Bld.CreateLoad(i32, aPtr, "a.val");
  Value *bVal = Bld.CreateLoad(i32, bPtr, "b.val");
  Value *d    = Bld.CreateSub(aVal, bVal, "d");

  // abs(d) = (d < 0) ? -d : d
  Value *isNeg = Bld.CreateICmpSLT(d, zero, "isneg");
  Value *negd  = Bld.CreateNSWNeg(d, "negd");
  Value *absd  = Bld.CreateSelect(isNeg, negd, d, "absd");

  // sum += absd
  Value *sum = Bld.CreateLoad(i32, sumAlloca, "sum.val");
  Value *sum2= Bld.CreateAdd(sum, absd, "sum.next");
  Bld.CreateStore(sum2, sumAlloca);
  Bld.CreateBr(LoopLatch);

  // latch: i++
  Bld.SetInsertPoint(LoopLatch);
  Value *i2 = Bld.CreateAdd(iVal, ConstantInt::get(i32, 1), "i.next");
  Bld.CreateStore(i2, iAlloca);
  Bld.CreateBr(LoopCond);

  // exit
  Bld.SetInsertPoint(Exit);
  Value *sumRet = Bld.CreateLoad(i32, sumAlloca, "sum.ret");
  Bld.CreateRet(sumRet);

  
  if (verifyFunction(*F, &errs())) {
    errs() << "verifyFunction failed for sad()\n";
  }
}

void runO3Pipeline(Module &M) {
  PassBuilder PB;
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;

  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O3);
  MPM.run(M, MAM);
}

void emitObjectFile(Module &M, TargetMachine &TM, const std::string &outPath) {
  M.setDataLayout(TM.createDataLayout());

  std::error_code EC;
  raw_fd_ostream OS(outPath, EC, sys::fs::OF_None);
  if (EC) {
    errs() << "open output failed: " << EC.message() << "\n";
    return;
  }

  legacy::PassManager PM;
  if (TM.addPassesToEmitFile(PM, OS, nullptr, CGFT_ObjectFile)) {
    errs() << "TargetMachine can't emit a file of this type\n";
    return;
  }
  PM.run(M);
  OS.flush();
}
