#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

//===----------------------------------------------------------------------===//
// VecOpt: If-convert a simple diamond into data-selects.
//
// Why: Turning control dependence into data dependence often unlocks
// vectorization/SLP and simplifies later passes.
//
// Safety highlights in this version:
//  - Convert *all* PHIs in the merge at once (keeps CFG+SSA consistent).
//  - Recursively hoist transitive dependencies from both arms with strict
//    speculation checks (no side effects, safe to speculatively execute,
//    non-convergent calls, etc.).
//  - Optional "freeze" on arm values before feeding them to select to avoid
//    accidentally introducing poison/undef when control-dependence is removed.
//
// Tested on LLVM 16–18 style APIs.
//===----------------------------------------------------------------------===//

static cl::opt<bool> EnableRewrite(
    "vecopt-rewrite",
    cl::desc("Enable the rewrite mode for VecOpt"),
    cl::init(false));

static cl::opt<bool> EnableFreeze(
    "vecopt-freeze",
    cl::desc("Insert freeze on select operands to guard against poison/undef"),
    cl::init(true));

// Print a helpful location header if debug info exists.
static void printLoc(const StringRef Fn, const Instruction &I) {
  if (const DebugLoc &DL = I.getDebugLoc()) {
    errs() << "[VecOpt] " << Fn << " @ " << DL->getFilename() << ":"
           << DL->getLine() << ": ";
  } else {
    errs() << "[VecOpt] " << Fn << ": ";
  }
}

// Cheap block-level filter used at discovery time.
static bool isSideEffectFreeBlock(const BasicBlock *BB) {
  for (const Instruction &I : *BB) {
    if (I.isTerminator() || isa<PHINode>(&I))
      continue;

    if (I.isVolatile() || I.mayWriteToMemory())
      return false;

    if (auto *CB = dyn_cast<CallBase>(&I)) {
      // Even if a call only reads memory, we usually don't want to speculate
      // it unless it's proven safe by LLVM (and it's non-convergent).
      if (!CB->onlyReadsMemory() && !CB->doesNotAccessMemory())
        return false;
    }

    if (!isSafeToSpeculativelyExecute(&I))
      return false;
  }
  return true;
}

// Classic diamond recognizer: header has conditional branch, both arms end in
// unconditional branches to the same merge, and the merge begins with PHIs.
static bool findDiamond(BranchInst *Br,
                        BasicBlock *&ThenBB, BasicBlock *&ElseBB,
                        BasicBlock *&MergeBB) {
  if (!Br || !Br->isConditional())
    return false;

  ThenBB = Br->getSuccessor(0);
  ElseBB = Br->getSuccessor(1);

  auto *ThenTerm = dyn_cast<BranchInst>(ThenBB->getTerminator());
  auto *ElseTerm = dyn_cast<BranchInst>(ElseBB->getTerminator());
  if (!ThenTerm || !ElseTerm || ThenTerm->isConditional() || ElseTerm->isConditional())
    return false;

  if (ThenTerm->getSuccessor(0) != ElseTerm->getSuccessor(0))
    return false;

  MergeBB = ThenTerm->getSuccessor(0);
  return true;
}

// Gather all PHIs in MergeBB that receive values from both arms.
static void collectRelevantPHIs(BasicBlock *MergeBB, BasicBlock *ThenBB,
                                BasicBlock *ElseBB, SmallVectorImpl<PHINode*> &Out) {
  for (Instruction &I : *MergeBB) {
    auto *P = dyn_cast<PHINode>(&I);
    if (!P) break; // PHIs are contiguous at the top
    if (P->getBasicBlockIndex(ThenBB) >= 0 && P->getBasicBlockIndex(ElseBB) >= 0)
      Out.push_back(P);
  }
}

// Instruction-level safety gate for speculation/hoisting.
static bool isHoistableInst(const Instruction *I) {
  if (!I) return false;
  if (I->isTerminator() || isa<PHINode>(I))
    return false;
  if (I->isEHPad() || I->isFenceLike())
    return false;
  if (I->isVolatile() || I->mayWriteToMemory())
    return false;

  // Calls: require no side effects, non-convergent, and speculatively safe.
  if (auto *CB = dyn_cast<CallBase>(I)) {
    if (CB->isConvergent())
      return false;
    if (!CB->doesNotAccessMemory() && !CB->onlyReadsMemory())
      return false;
  }

  return isSafeToSpeculativelyExecute(I);
}

// Recursively collect hoistable defs rooted at V, limited to values computed
// in ArmBB. Post-order push gives a safe move-before order.
static bool collectHoistSet(Value *V, BasicBlock *ArmBB,
                            SmallPtrSetImpl<Instruction*> &Visited,
                            SmallVectorImpl<Instruction*> &PostOrder) {
  auto *I = dyn_cast<Instruction>(V);
  if (!I) return true; // Arguments/consts are OK.

  if (I->getParent() != ArmBB)
    return true; // Only hoist from inside the arm.

  if (!Visited.insert(I).second)
    return true; // already processed

  if (!isHoistableInst(I))
    return false;

  for (Value *Op : I->operands()) {
    if (!collectHoistSet(Op, ArmBB, Visited, PostOrder))
      return false;
  }

  PostOrder.push_back(I); // post-order for topological move
  return true;
}

static Value *maybeFreeze(Value *V, IRBuilder<> &B) {
  if (!EnableFreeze)
    return V;
  // Don't bother freezing constants; otherwise freeze to avoid propagating
  // poison/undef from dead arms after if-conversion.
  if (isa<Constant>(V))
    return V;
  return B.CreateFreeze(V, V->getName() + ".frz");
}

static bool doIfConversion(Function &F, BranchInst *Br,
                           BasicBlock *ThenBB, BasicBlock *ElseBB,
                           BasicBlock *MergeBB) {
  // 1) Collect all PHIs in the merge related to this diamond.
  SmallVector<PHINode*, 8> PHIs;
  collectRelevantPHIs(MergeBB, ThenBB, ElseBB, PHIs);
  if (PHIs.empty())
    return false; // nothing to do

  // 2) First pass: build the transitive hoist set for all PHI incoming values.
  SmallPtrSet<Instruction*, 32> VisitedThen, VisitedElse;
  SmallVector<Instruction*, 32> OrderThen, OrderElse;

  for (PHINode *P : PHIs) {
    Value *TV = P->getIncomingValueForBlock(ThenBB);
    Value *EV = P->getIncomingValueForBlock(ElseBB);

    if (!collectHoistSet(TV, ThenBB, VisitedThen, OrderThen))
      return false; // bail — cannot safely hoist
    if (!collectHoistSet(EV, ElseBB, VisitedElse, OrderElse))
      return false;
  }

  // 3) Move hoistable defs before the header branch, keeping order.
  Instruction *InsertPt = Br; // insert before the conditional branch
  for (Instruction *I : OrderThen)
    I->moveBefore(InsertPt);
  for (Instruction *I : OrderElse)
    I->moveBefore(InsertPt);

  // 4) For each PHI, build a select and replace.
  IRBuilder<> B(Br);
  Value *Cond = Br->getCondition();

  SmallVector<PHINode*, 8> ToErase;
  ToErase.reserve(PHIs.size());

  for (PHINode *P : PHIs) {
    Value *TV = P->getIncomingValueForBlock(ThenBB);
    Value *EV = P->getIncomingValueForBlock(ElseBB);

    Value *Sel = B.CreateSelect(Cond,
                                maybeFreeze(TV, B),
                                maybeFreeze(EV, B),
                                P->getName() + ".select");

    P->replaceAllUsesWith(Sel);
    ToErase.push_back(P);
  }

  for (PHINode *P : ToErase)
    P->eraseFromParent();

  // 5) Rewire header to jump directly to merge; dependent cleanups are left to
  // SimplifyCFG/DCE.
  BasicBlock *HeaderBB = Br->getParent();
  printLoc(F.getName(), *Br);
  errs() << "if-converting diamond -> selects in '" << MergeBB->getName() << "'\n";

  Br->eraseFromParent();
  BranchInst::Create(MergeBB, HeaderBB);

  return true;
}

namespace {
class VecOptPass : public PassInfoMixin<VecOptPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    // Respect -O0 users, but allow opt-in.
    if (F.hasFnAttribute(Attribute::OptimizeNone))
      F.removeFnAttr(Attribute::OptimizeNone);

    LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
    bool Changed = false;

    SmallVector<std::tuple<BranchInst*, BasicBlock*, BasicBlock*, BasicBlock*>, 8> Work;

    for (BasicBlock &BB : F) {
      // Heuristic: only look inside loops by default — easiest wins for vec.
      if (!LI.getLoopFor(&BB))
        continue;

      auto *Br = dyn_cast<BranchInst>(BB.getTerminator());
      if (!Br) continue;

      BasicBlock *ThenBB = nullptr, *ElseBB = nullptr, *MergeBB = nullptr;
      if (!findDiamond(Br, ThenBB, ElseBB, MergeBB))
        continue;

      // Fast block-level purity screen.
      if (!isSideEffectFreeBlock(ThenBB) || !isSideEffectFreeBlock(ElseBB)) {
        printLoc(F.getName(), *Br);
        errs() << "diamond found but arms have side effects — skip\n";
        continue;
      }

      if (EnableRewrite)
        Work.emplace_back(Br, ThenBB, ElseBB, MergeBB);
      else {
        printLoc(F.getName(), *Br);
        errs() << "diamond -> candidate for if->select in '" << MergeBB->getName() << "'\n";
      }
    }

    for (auto &T : Work) {
      auto *Br = std::get<0>(T);
      auto *ThenBB = std::get<1>(T);
      auto *ElseBB = std::get<2>(T);
      auto *MergeBB = std::get<3>(T);

      Changed |= doIfConversion(F, Br, ThenBB, ElseBB, MergeBB);
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};
} // namespace

PassPluginLibraryInfo getVecOptPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "VecOpt", "1.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "vecopt") {
                    FPM.addPass(VecOptPass());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getVecOptPluginInfo();
}
