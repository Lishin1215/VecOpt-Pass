#include "codegen.h"
#include "parser.h"
#include "ast.h"
#include "preprocessor.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>

#include <fstream>
#include <sstream>

using namespace llvm;

static cl::opt<std::string> InPath("input", cl::desc("C or preprocessed C (.c/.i) input"), cl::value_desc("file"));
static cl::opt<std::string> IncludeDir("I", cl::desc("Add include search dir"), cl::value_desc("dir"));
static cl::opt<std::string> OutObj("o", cl::desc("Output object file"), cl::init("a.o"));
static cl::opt<bool> EmitObj("c", cl::desc("Emit object file (.o)"));
static cl::opt<bool> OptO3("O3", cl::desc("Enable O3 pipeline (default on)"), cl::init(true));
static cl::opt<bool> EmitSAD("emit-sad", cl::desc("Emit built-in sad() kernel (for debug)"));

static bool endsWith(const std::string& s, const char* suf){
  size_t n = std::char_traits<char>::length(suf);
  return s.size() >= n && s.compare(s.size()-n, n, suf) == 0;
}

int main(int argc, char** argv) {
  cl::ParseCommandLineOptions(argc, argv, "veclangc – tiny C frontend with mini-preprocessor\n");

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  LLVMContext Ctx;
  auto Mod = std::make_unique<Module>("veclangc", Ctx);

  std::string triple = sys::getDefaultTargetTriple();
  Mod->setTargetTriple(triple);
  auto TM = createTargetMachineFromTriple(triple);
  if (!TM) return 1;
  Mod->setDataLayout(TM->createDataLayout());

  if (EmitSAD) {
    buildSADKernelIR(*Mod);
  } else {
    if (InPath.empty()){ errs() << "need --input <file.c|file.i>\n"; return 1; }

    std::string sourceText;

    if (endsWith(InPath, ".c")) {
      // Run tiny preprocessor
      Preprocessor PP;
      if (!IncludeDir.empty()) PP.addIncludeDir(IncludeDir);
      // Also add the input file's directory as first search path for "..."
      // (PP handles that internally when resolving includes)
      try {
        sourceText = PP.run(InPath);
      } catch (const std::exception& ex) {
        errs() << "preprocess failed: " << ex.what() << "\n";
        return 1;
      }
    } else {
      // Treat as already-preprocessed .i
      std::ifstream ifs(InPath);
      if (!ifs){ errs() << "cannot open " << InPath << "\n"; return 1; }
      std::stringstream ss; ss << ifs.rdbuf();
      sourceText = ss.str();
    }

    Parser P(std::move(sourceText));
    FuncAST F = P.parseFunction();    // C text -> AST
    buildFromAST(*Mod, F);            // AST -> IR
  }

  if (OptO3) runO3Pipeline(*Mod);
  if (EmitObj) emitObjectFile(*Mod, *TM, OutObj);
  return 0;
}
