//===- VecOpt.cpp ------------------------------------------------*- C++ -*-===//
//
// VecOpt: If-convert simple diamonds (inside loops) into selects to help
// LoopVectorizer / SLPVectorizer.
//
// Safer version with guards:
//  - Closed diamond only (no extra predecessors).
//  - Convert all relevant PHIs in the merge block at once.
//  - Hoist transitive defs from both arms (speculatively safe + non-convergent).
//  - Optional freeze() on select operands.
//  - Gates: vectorization-friendly types (i32/f32/f64), no-load arms (by default),
//           cap hoisted insts, skip highly-biased branches, skip loop-invariant
//           conditions, only inside loops.
//  - Registered at VectorizerStart so LV/SLP can benefit.
//
// Tested with LLVM 16–18 style APIs.
//
//===----------------------------------------------------------------------===//

#include <cstdlib> // std::getenv

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

//------------------------------------------------------------------------------
// Options
//------------------------------------------------------------------------------
static cl::opt<bool> EnableRewrite(
    "vecopt-rewrite",
    cl::desc("Enable rewrite mode for VecOpt"),
    cl::init(true));

static cl::opt<bool> EnableFreeze(
    "vecopt-freeze",
    cl::desc("Insert freeze before select operands to block poison/undef"),
    cl::init(true));

static cl::opt<unsigned> MaxArmInsts(
    "vecopt-max-arm",
    cl::desc("Maximum total hoisted instructions across both arms"),
    cl::init(6));

static cl::opt<bool> AllowLoadHoist(
    "vecopt-allow-load-hoist",
    cl::desc("Allow hoisting of loads from arms (may increase memory traffic)"),
    cl::init(false));

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------
static void printLoc(StringRef Fn, const Instruction &I) {
  if (const DebugLoc &DL = I.getDebugLoc()) {
    errs() << "[VecOpt] " << Fn << " @ " << DL->getFilename()
           << ":" << DL->getLine() << ": ";
  } else {
    errs() << "[VecOpt] " << Fn << ": ";
  }
}

static bool isSideEffectFreeBlock(const BasicBlock *BB) {
  for (const Instruction &I : *BB) {
    if (I.isTerminator() || isa<PHINode>(&I))
      continue;
    if (I.isVolatile() || I.mayWriteToMemory())
      return false;
    if (auto *CB = dyn_cast<CallBase>(&I)) {
      if (CB->isConvergent())
        return false;
      if (!CB->doesNotAccessMemory() && !CB->onlyReadsMemory())
        return false;
    }
    if (!isSafeToSpeculativelyExecute(&I))
      return false;
  }
  return true;
}

// Some LLVM builds don’t expose hasNPredecessors(); count preds ourselves.
static bool hasExactlyNPreds(const BasicBlock *BB, unsigned N) {
  unsigned C = 0;
  for (auto *P : predecessors(BB)) {
    (void)P;
    if (++C > N) return false;
  }
  return C == N;
}

// Require a closed diamond: Then/Else only from Header, Merge only from Then/Else.
static bool isClosedDiamond(BasicBlock *Header, BasicBlock *ThenBB,
                            BasicBlock *ElseBB, BasicBlock *MergeBB) {
  if (!hasExactlyNPreds(ThenBB, 1) || *pred_begin(ThenBB) != Header)
    return false;
  if (!hasExactlyNPreds(ElseBB, 1) || *pred_begin(ElseBB) != Header)
    return false;
  if (!hasExactlyNPreds(MergeBB, 2))
    return false;
  for (BasicBlock *P : predecessors(MergeBB))
    if (P != ThenBB && P != ElseBB)
      return false;
  return true;
}

// Detect a classic diamond
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

// Collect PHIs in MergeBB that merge values from both arms
static void collectRelevantPHIs(BasicBlock *MergeBB, BasicBlock *ThenBB,
                                BasicBlock *ElseBB, SmallVectorImpl<PHINode*> &Out) {
  for (Instruction &I : *MergeBB) {
    auto *P = dyn_cast<PHINode>(&I);
    if (!P) break;
    if (P->getBasicBlockIndex(ThenBB) >= 0 && P->getBasicBlockIndex(ElseBB) >= 0)
      Out.push_back(P);
  }
}

// Instruction-level safety for speculation
static bool isHoistableInst(const Instruction *I) {
  if (!I) return false;
  if (I->isTerminator() || isa<PHINode>(I)) return false;
  if (I->isEHPad() || I->isFenceLike()) return false;
  if (I->isVolatile() || I->mayWriteToMemory()) return false;
  if (auto *CB = dyn_cast<CallBase>(I)) {
    if (CB->isConvergent()) return false;
    if (!CB->doesNotAccessMemory() && !CB->onlyReadsMemory())
      return false;
  }
  return isSafeToSpeculativelyExecute(I);
}

// Collect hoistable defs rooted at V from inside ArmBB
static bool collectHoistSet(Value *V, BasicBlock *ArmBB,
                            SmallPtrSetImpl<Instruction*> &Visited,
                            SmallVectorImpl<Instruction*> &PostOrder) {
  auto *I = dyn_cast<Instruction>(V);
  if (!I) return true;
  if (I->getParent() != ArmBB) return true;
  if (!Visited.insert(I).second) return true;
  if (!isHoistableInst(I)) return false;
  for (Value *Op : I->operands())
    if (!collectHoistSet(Op, ArmBB, Visited, PostOrder))
      return false;
  PostOrder.push_back(I);
  return true;
}

static bool containsLoad(ArrayRef<Instruction*> Seq) {
  for (Instruction *I : Seq)
    if (isa<LoadInst>(I)) return true;
  return false;
}

static bool isVecFriendlyTy(Type *T) {
  if (auto *IT = dyn_cast<IntegerType>(T))
    return IT->getBitWidth() == 32; // i32 only
  return T->isFloatTy() || T->isDoubleTy();
}

// Skip highly biased branches (select would execute both arms)
static bool isHighlyBiased(BranchInst *Br, double Thresh = 8.0) {
  if (!Br || !Br->isConditional()) return false;
  if (MDNode *Prof = Br->getMetadata(LLVMContext::MD_prof)) {
    if (Prof->getNumOperands() >= 3) {
      if (auto *Tag = dyn_cast<MDString>(Prof->getOperand(0)))
        if (Tag->getString() == "branch_weights") {
          auto *T = mdconst::dyn_extract<ConstantInt>(Prof->getOperand(1));
          auto *F = mdconst::dyn_extract<ConstantInt>(Prof->getOperand(2));
          if (!T || !F) return false;
          uint64_t TW = T->getZExtValue(), FW = F->getZExtValue();
          if (TW == 0 || FW == 0) return true;
          double r = (double)std::max(TW, FW) / (double)std::min(TW, FW);
          return r >= Thresh;
        }
    }
  }
  return false;
}

static Value *maybeFreeze(Value *V, IRBuilder<> &B) {
  if (!EnableFreeze) return V;
  if (isa<Constant>(V)) return V;
  return B.CreateFreeze(V, V->getName() + ".frz");
}

//------------------------------------------------------------------------------
// Core conversion
//------------------------------------------------------------------------------
static bool doIfConversion(Function &F, BranchInst *Br,
                           BasicBlock *ThenBB, BasicBlock *ElseBB,
                           BasicBlock *MergeBB) {
  SmallVector<PHINode*, 8> PHIs;
  collectRelevantPHIs(MergeBB, ThenBB, ElseBB, PHIs);
  if (PHIs.empty()) return false;

  // Type gate: must be vectorization-friendly and consistent
  for (PHINode *P : PHIs) {
    Value *TV = P->getIncomingValueForBlock(ThenBB);
    Value *EV = P->getIncomingValueForBlock(ElseBB);
    if (P->getType() != TV->getType() || P->getType() != EV->getType())
      return false;
    if (!isVecFriendlyTy(P->getType()))
      return false;
  }

  // Collect hoist sets
  SmallPtrSet<Instruction*, 32> VisitedThen, VisitedElse;
  SmallVector<Instruction*, 32> OrderThen, OrderElse;
  for (PHINode *P : PHIs) {
    if (!collectHoistSet(P->getIncomingValueForBlock(ThenBB), ThenBB,
                         VisitedThen, OrderThen))
      return false;
    if (!collectHoistSet(P->getIncomingValueForBlock(ElseBB), ElseBB,
                         VisitedElse, OrderElse))
      return false;
  }

  // Cost gate
  unsigned Total = OrderThen.size() + OrderElse.size();
  if (Total > MaxArmInsts) return false;
  if (!AllowLoadHoist && (containsLoad(OrderThen) || containsLoad(OrderElse)))
    return false;

  // Hoist
  Instruction *InsertPt = Br;
  for (Instruction *I : OrderThen) I->moveBefore(InsertPt);
  for (Instruction *I : OrderElse) I->moveBefore(InsertPt);

  // Replace PHIs with selects
  IRBuilder<> B(Br);
  Value *Cond = Br->getCondition();
  SmallVector<PHINode*, 8> ToErase;
  for (PHINode *P : PHIs) {
    Value *TV = P->getIncomingValueForBlock(ThenBB);
    Value *EV = P->getIncomingValueForBlock(ElseBB);
    Value *Sel = B.CreateSelect(Cond, maybeFreeze(TV, B), maybeFreeze(EV, B),
                                P->getName() + ".select");
    P->replaceAllUsesWith(Sel);
    ToErase.push_back(P);
  }
  for (PHINode *P : ToErase) P->eraseFromParent();

  // Rewire header to merge
  BasicBlock *HeaderBB = Br->getParent();
  printLoc(F.getName(), *Br);
  errs() << "if-converting diamond -> selects in '" << MergeBB->getName() << "'\n";
  Br->eraseFromParent();
  BranchInst::Create(MergeBB, HeaderBB);
  return true;
}

//------------------------------------------------------------------------------
// Pass
//------------------------------------------------------------------------------
namespace {
class VecOptPass : public PassInfoMixin<VecOptPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    if (F.hasFnAttribute(Attribute::OptimizeNone))
      F.removeFnAttr(Attribute::OptimizeNone);

    if (const char *Env = std::getenv("VECOPT_REWRITE"))
      EnableRewrite = StringRef(Env) != "0";

    LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
    bool Changed = false;
    SmallVector<std::tuple<BranchInst*, BasicBlock*, BasicBlock*, BasicBlock*>, 8> Work;

    for (BasicBlock &BB : F) {
      Loop *L = LI.getLoopFor(&BB);
      if (!L) continue;

      auto *Br = dyn_cast<BranchInst>(BB.getTerminator());
      if (!Br || !Br->isConditional())
        continue;

      BasicBlock *ThenBB = nullptr, *ElseBB = nullptr, *MergeBB = nullptr;
      if (!findDiamond(Br, ThenBB, ElseBB, MergeBB))
        continue;
      if (!isClosedDiamond(Br->getParent(), ThenBB, ElseBB, MergeBB))
        continue;

      // now it's safe to touch Br->getCondition()
      if (L->isLoopInvariant(Br->getCondition())) {
        printLoc(F.getName(), *Br);
        errs() << "skip: loop-invariant condition\n";
        continue;
      }

      if (isHighlyBiased(Br)) {
        printLoc(F.getName(), *Br);
        errs() << "skip: highly-biased branch\n";
        continue;
      }

      if (!isSideEffectFreeBlock(ThenBB) || !isSideEffectFreeBlock(ElseBB)) {
        printLoc(F.getName(), *Br);
        errs() << "diamond with side effects — skip\n";
        continue;
      }

      if (EnableRewrite)
        Work.emplace_back(Br, ThenBB, ElseBB, MergeBB);
      else {
        printLoc(F.getName(), *Br);
        errs() << "diamond -> candidate for if->select in '"
              << MergeBB->getName() << "'\n";
      }
    }


    for (auto &T : Work) {
      Changed |= doIfConversion(F, std::get<0>(T), std::get<1>(T),
                                std::get<2>(T), std::get<3>(T));
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};
} // namespace

//------------------------------------------------------------------------------
// Plugin registration
//------------------------------------------------------------------------------
PassPluginLibraryInfo getVecOptPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "VecOpt", "1.2",
    [](PassBuilder &PB) {
      // NOTE: LLVM 18 callback has signature (FPM&, OptimizationLevel)
      PB.registerVectorizerStartEPCallback(
        [&](FunctionPassManager &FPM, OptimizationLevel) {
          FPM.addPass(VecOptPass());
        });

      PB.registerPipelineParsingCallback(
        [&](StringRef Name, FunctionPassManager &FPM,
            ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "vecopt") {
            FPM.addPass(VecOptPass());
            return true;
          }
          return false;
        });
    }
  };
}

extern "C" ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getVecOptPluginInfo();
}
