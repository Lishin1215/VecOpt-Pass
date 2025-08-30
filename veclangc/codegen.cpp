#include "codegen.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>

using namespace llvm;

std::unique_ptr<TargetMachine> createTargetMachineFromTriple(const std::string &tripleStr) {
  std::string Error;
  const Target *T = TargetRegistry::lookupTarget(tripleStr, Error);
  if (!T) {
    errs() << "lookupTarget failed: " << Error << "\n";
    return nullptr;
  }
  std::string CPU = sys::getHostCPUName().str();
  std::string Features; // keep empty or query Host for defaults
  TargetOptions opt;
  auto RM = std::optional<Reloc::Model>();
  std::unique_ptr<TargetMachine> TM(
      T->createTargetMachine(tripleStr, CPU, Features, opt, RM));
  return TM;
}

// Debug-only: build a fixed IR for int sad(const int*, const int*, int)
void buildSADKernelIR(Module &M) {
  LLVMContext &C = M.getContext();
  Type *i32 = Type::getInt32Ty(C);
  Type *pi32 = PointerType::getUnqual(i32);

  FunctionType *FT = FunctionType::get(i32, {pi32, pi32, i32}, false);
  llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "sad", M);

  auto AI = F->arg_begin();
  Value *A      = &*AI++; A->setName("a");
  Value *Bparam = &*AI++; Bparam->setName("b");   // <-- renamed to avoid clash
  Value *N      = &*AI++; N->setName("n");

  BasicBlock *Entry    = BasicBlock::Create(C, "entry",    F);
  BasicBlock *LoopCond = BasicBlock::Create(C, "loop.cond",F);
  BasicBlock *LoopBody = BasicBlock::Create(C, "loop.body",F);
  BasicBlock *LoopInc  = BasicBlock::Create(C, "loop.inc", F);
  BasicBlock *Exit     = BasicBlock::Create(C, "exit",     F);

  IRBuilder<> B(Entry);
  Value *zero = ConstantInt::get(i32, 0);
  AllocaInst *iAlloca   = B.CreateAlloca(i32, nullptr, "i");
  AllocaInst *sumAlloca = B.CreateAlloca(i32, nullptr, "sum");
  B.CreateStore(zero, iAlloca);
  B.CreateStore(zero, sumAlloca);
  B.CreateBr(LoopCond);

  B.SetInsertPoint(LoopCond);
  Value *iVal = B.CreateLoad(i32, iAlloca, "i.val");
  Value *cmp  = B.CreateICmpSLT(iVal, N, "cmp");
  B.CreateCondBr(cmp, LoopBody, Exit);

  B.SetInsertPoint(LoopBody);
  Value *aPtr = B.CreateInBoundsGEP(i32, A,      iVal, "a.idx");
  Value *bPtr = B.CreateInBoundsGEP(i32, Bparam, iVal, "b.idx"); // <-- use Bparam
  Value *aVal = B.CreateLoad(i32, aPtr, "a.val");
  Value *bVal = B.CreateLoad(i32, bPtr, "b.val");
  Value *d    = B.CreateSub(aVal, bVal, "d");
  Value *isNeg= B.CreateICmpSLT(d, zero, "isneg");
  Value *negd = B.CreateNSWNeg(d, "negd");
  Value *absd = B.CreateSelect(isNeg, negd, d, "absd");
  Value *sum  = B.CreateLoad(i32, sumAlloca, "sum.val");
  Value *sum2 = B.CreateAdd(sum, absd, "sum.next");
  B.CreateStore(sum2, sumAlloca);
  B.CreateBr(LoopInc);

  B.SetInsertPoint(LoopInc);
  Value *i2 = B.CreateAdd(iVal, ConstantInt::get(i32, 1), "i.next");
  B.CreateStore(i2, iAlloca);
  B.CreateBr(LoopCond);

  B.SetInsertPoint(Exit);
  Value *sumRet = B.CreateLoad(i32, sumAlloca, "sum.ret");
  B.CreateRet(sumRet);

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
  if (TM.addPassesToEmitFile(PM, OS, nullptr, llvm::CodeGenFileType::ObjectFile)) {
    errs() << "TargetMachine can't emit a file of this type\n";
    return;
  }
  PM.run(M);
  OS.flush();
}
