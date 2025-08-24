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

static cl::opt<bool> EnableRewrite(
    "vecopt-rewrite",
    cl::desc("Enable the rewrite mode for VecOpt"),
    cl::init(false)
);

static void printLoc(const StringRef Fn, const Instruction &I) {
  if (const DebugLoc &DL = I.getDebugLoc()) {
    errs() << "[VecOpt] " << Fn
           << " @ " << DL->getFilename() << ":" << DL->getLine() << ": ";
  } else {
    errs() << "[VecOpt] " << Fn << ": ";
  }
}

static bool isSideEffectFree(const BasicBlock *BB) {
  for (const Instruction &I : *BB) {
    if (I.isTerminator() || isa<PHINode>(&I)) continue;
    if (I.mayWriteToMemory() || I.isVolatile()) return false;
    if (auto *CB = dyn_cast<CallBase>(&I)) {
      if (!CB->onlyReadsMemory() && !CB->doesNotAccessMemory()) return false;
    }
    if (!isSafeToSpeculativelyExecute(&I)) return false;
  }
  return true;
}

static bool findDiamond(BranchInst *Br,
                        BasicBlock *&ThenBB, BasicBlock *&ElseBB,
                        BasicBlock *&MergeBB, PHINode *&Phi) {
  if (!Br || !Br->isConditional()) return false;
  ThenBB = Br->getSuccessor(0);
  ElseBB = Br->getSuccessor(1);
  auto *ThenTerm = dyn_cast<BranchInst>(ThenBB->getTerminator());
  auto *ElseTerm = dyn_cast<BranchInst>(ElseBB->getTerminator());
  if (!ThenTerm || !ElseTerm || ThenTerm->isConditional() || ElseTerm->isConditional()) return false;
  if (ThenTerm->getSuccessor(0) != ElseTerm->getSuccessor(0)) return false;
  MergeBB = ThenTerm->getSuccessor(0);
  Phi = nullptr;
  for (Instruction &I : *MergeBB) {
    if (auto *P = dyn_cast<PHINode>(&I)) {
      if (P->getBasicBlockIndex(ThenBB) >= 0 && P->getBasicBlockIndex(ElseBB) >= 0) {
        Phi = P;
        return true;
      }
    } else {
      break;
    }
  }
  return false;
}

// --- THIS IS THE CORRECTED FUNCTION ---
static void doIfConversion(BranchInst *Br, PHINode *Phi) {
    BasicBlock *HeaderBB = Br->getParent();
    BasicBlock *ThenBB = Br->getSuccessor(0);
    BasicBlock *ElseBB = Br->getSuccessor(1);
    BasicBlock *MergeBB = Phi->getParent();

    Value *ThenValue = Phi->getIncomingValueForBlock(ThenBB);
    Value *ElseValue = Phi->getIncomingValueForBlock(ElseBB);

    // Hoist the instruction that computes ThenValue and its direct dependencies.
    if (auto *ThenInst = dyn_cast<Instruction>(ThenValue)) {
        if (ThenInst->getParent() == ThenBB) {
            // First, move any operands that are also in ThenBB. This handles dependencies.
            for (Value *Operand : ThenInst->operands()) {
                if (auto *OpInst = dyn_cast<Instruction>(Operand)) {
                    if (OpInst->getParent() == ThenBB) {
                        OpInst->moveBefore(Br);
                    }
                }
            }
            // Then, move the instruction itself.
            ThenInst->moveBefore(Br);
        }
    }

    // Hoist the instruction that computes ElseValue and its direct dependencies.
    if (auto *ElseInst = dyn_cast<Instruction>(ElseValue)) {
        if (ElseInst->getParent() == ElseBB) {
            for (Value *Operand : ElseInst->operands()) {
                if (auto *OpInst = dyn_cast<Instruction>(Operand)) {
                    if (OpInst->getParent() == ElseBB) {
                        OpInst->moveBefore(Br);
                    }
                }
            }
            ElseInst->moveBefore(Br);
        }
    }

    // Now that the definitions are hoisted into the header, they dominate the branch.
    // We can now safely create the select instruction before the branch.
    IRBuilder<> Builder(Br);
    Value *Select = Builder.CreateSelect(Br->getCondition(), ThenValue, ElseValue,
                                         Phi->getName() + ".select");

    // The PHI node is now redundant. Replace its uses and remove it.
    Phi->replaceAllUsesWith(Select);
    Phi->eraseFromParent();

    // Rewire the control flow to be unconditional.
    Br->eraseFromParent();
    BranchInst::Create(MergeBB, HeaderBB);

    // The ThenBB and ElseBB are now dead code and will be cleaned up by a DCE pass.
}


namespace {
class VecOptPass : public PassInfoMixin<VecOptPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    if (F.hasFnAttribute(Attribute::OptimizeNone)) {
        F.removeFnAttr(Attribute::OptimizeNone);
    }

    LoopInfo *LI = &FAM.getResult<LoopAnalysis>(F);
    bool IRWasModified = false;
    
    std::vector<std::pair<BranchInst*, PHINode*>> Candidates;

    for (BasicBlock &BB : F) {
      if (!LI->getLoopFor(&BB)) continue;

      if (auto *Br = dyn_cast<BranchInst>(BB.getTerminator())) {
        BasicBlock *ThenBB = nullptr, *ElseBB = nullptr, *MergeBB = nullptr;
        PHINode *Phi = nullptr;
        if (findDiamond(Br, ThenBB, ElseBB, MergeBB, Phi)) {
          bool armsArePure = isSideEffectFree(ThenBB) && isSideEffectFree(ElseBB);

          if (EnableRewrite && armsArePure) {
            Candidates.push_back({Br, Phi});
          } else {
            printLoc(F.getName(), *Br);
            errs() << "branch-diamond -> merge '" << Phi->getParent()->getName() << "'; "
                 << (armsArePure ? "candidate for if->select.\n"
                                 : "NOT safe to if-convert (side effects).\n");
          }
        }
      }
    }

    for (auto const& [Br, Phi] : Candidates) {
      printLoc(F.getName(), *Br);
      errs() << "rewriting to select.\n";
      doIfConversion(Br, Phi);
      IRWasModified = true;
    }

    if (IRWasModified) return PreservedAnalyses::none();
    return PreservedAnalyses::all();
  }
};
} // namespace

PassPluginLibraryInfo getVecOptPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "VecOpt", "1.0",
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
    }
  };
}

extern "C" ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getVecOptPluginInfo();
}
