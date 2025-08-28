#include "codegen.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

static cl::opt<std::string> OutObj("o", cl::desc("Output object file"), cl::init("a.o"));
static cl::opt<bool> EmitObj("c", cl::desc("Emit object file (.o)"));
static cl::opt<bool> EmitSAD("emit-sad", cl::desc("Emit built-in sad() kernel"));

int main(int argc, char** argv) {
  cl::ParseCommandLineOptions(argc, argv, "veclangc – tiny C frontend (step1)\n");

  InitializeAllTargetInfos();
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmParsers();
  InitializeAllAsmPrinters();

  LLVMContext Ctx;
  auto Mod = std::make_unique<Module>("veclangc", Ctx);

  std::string triple = sys::getDefaultTargetTriple();
  Mod->setTargetTriple(triple);
  auto TM = createTargetMachineFromTriple(triple);
  if (!TM) return 1;
  Mod->setDataLayout(TM->createDataLayout());

  if (!EmitSAD) {
    errs() << "For step1, use --emit-sad to generate sad() kernel.\n";
    return 1;
  }

  buildSADKernelIR(*Mod);
  runO3Pipeline(*Mod);

  if (EmitObj) {
    emitObjectFile(*Mod, *TM, OutObj);
  } else {
    errs() << "Nothing to do (pass -c -o out.o)\n";
  }
  return 0;
}
