#include "ast.h"
#include "codegen.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <unordered_map>

using namespace llvm;

namespace {

struct CG {
  Module &M; IRBuilder<> B;
  Type *i32; Type *pi32;
  llvm::Function *LF=nullptr;
  std::unordered_map<std::string, AllocaInst*> locals;
  Value *N=nullptr, *A=nullptr, *Bptr=nullptr;

  CG(Module &M): M(M), B(M.getContext()),
    i32(Type::getInt32Ty(M.getContext())),
    pi32(PointerType::getUnqual(i32)) {}

  Value* emitExpr(Expr* e){
    if (auto *n = dynamic_cast<NumberExpr*>(e)) return ConstantInt::get(i32, n->v);
    if (auto *v = dynamic_cast<VarExpr*>(e)) {
      // Handle parameters first
      if (v->name == "n")  return N;          // parameter n
      if (v->name == "a")  return A;          // rarely used alone, but safe-guard
      if (v->name == "b")  return Bptr;       // rarely used alone, but safe-guard
      // Then local variables
      auto it = locals.find(v->name);
      if (it == locals.end())
        throw std::runtime_error(std::string("unknown variable: ") + v->name);
      return B.CreateLoad(i32, it->second);
    }
    if (auto *ix= dynamic_cast<IndexExpr*>(e)){
      Value *base = (ix->base=="a") ? A : (ix->base=="b") ? Bptr : nullptr;
      auto idx = emitExpr(ix->idx.get());
      auto ptr = B.CreateInBoundsGEP(i32, base, idx);
      return B.CreateLoad(i32, ptr);
    }
    if (auto *ne= dynamic_cast<NegExpr*>(e)){
      auto x = emitExpr(ne->x.get());
      return B.CreateNSWNeg(x);
    }
    if (auto *be= dynamic_cast<BinExpr*>(e)){
      Value *x = emitExpr(be->a.get()), *y = emitExpr(be->b.get());
      switch(be->op){
        case BinOp::Add: return B.CreateAdd(x,y);
        case BinOp::Sub: return B.CreateSub(x,y);
        case BinOp::Mul: return B.CreateMul(x,y);
        case BinOp::Div: return B.CreateSDiv(x,y);
        case BinOp::Mod: return B.CreateSRem(x,y);
        case BinOp::LT:  return B.CreateICmpSLT(x,y);
        case BinOp::LE:  return B.CreateICmpSLE(x,y);
        case BinOp::GT:  return B.CreateICmpSGT(x,y);
        case BinOp::GE:  return B.CreateICmpSGE(x,y);
        case BinOp::EQ:  return B.CreateICmpEQ(x,y);
        case BinOp::NE:  return B.CreateICmpNE(x,y);
      }
    }
    return nullptr;
  }

  void emitAssign(AssignStmt* s){
    if (auto *v = dynamic_cast<VarExpr*>(s->lhs.get())) {
      auto rhs = emitExpr(s->rhs.get());
      B.CreateStore(rhs, locals.at(v->name));
      return;
    }
    if (auto *ix = dynamic_cast<IndexExpr*>(s->lhs.get())){
      Value *base = (ix->base=="a") ? A : (ix->base=="b") ? Bptr : nullptr;
      auto idx = emitExpr(ix->idx.get());
      auto ptr = B.CreateInBoundsGEP(i32, base, idx);
      auto rhs = emitExpr(s->rhs.get());
      B.CreateStore(rhs, ptr);
      return;
    }
  }

  void emit(const FuncAST &Fn){
    // Create function header: int sad(const int*, const int*, int)
    FunctionType *FT = FunctionType::get(i32, {pi32, pi32, i32}, false);
    LF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "sad", &M);
    auto AI = LF->arg_begin();
    A    = &*AI++; A->setName("a");
    Bptr = &*AI++; Bptr->setName("b");
    N    = &*AI++; N->setName("n");

    BasicBlock *entry = BasicBlock::Create(M.getContext(), "entry", LF);
    B.SetInsertPoint(entry);

    // Walk body statements
    for (auto &uptr : Fn.body) {
      if (auto *decl = dynamic_cast<DeclStmt*>(uptr.get())){
        auto slot = B.CreateAlloca(i32, nullptr, decl->name);
        locals[decl->name] = slot;
        auto init = emitExpr(decl->init.get());
        B.CreateStore(init, slot);
      } else if (auto *as = dynamic_cast<AssignStmt*>(uptr.get())){
        emitAssign(as);
      } else if (auto *add = dynamic_cast<AddAssignStmt*>(uptr.get())){
        auto rhs = emitExpr(add->rhs.get());
        auto cur = B.CreateLoad(i32, locals.at(add->name));
        B.CreateStore(B.CreateAdd(cur, rhs), locals.at(add->name));
      } else if (auto *ifs = dynamic_cast<IfStmt*>(uptr.get())){
        auto cond = emitExpr(ifs->cond.get());
        auto thenBB = BasicBlock::Create(M.getContext(), "then", LF);
        auto contBB = BasicBlock::Create(M.getContext(), "if.cont", LF);
        B.CreateCondBr(cond, thenBB, contBB);
        B.SetInsertPoint(thenBB);
        for (auto &s2 : ifs->thenStmts){
          if (auto *as2 = dynamic_cast<AssignStmt*>(s2.get())) emitAssign(as2);
        }
        B.CreateBr(contBB);
        B.SetInsertPoint(contBB);
      } else if (auto *fs = dynamic_cast<ForStmt*>(uptr.get())){
        // Build loop blocks
        auto condBB = BasicBlock::Create(M.getContext(), "for.cond", LF);
        auto bodyBB = BasicBlock::Create(M.getContext(), "for.body", LF);
        auto incBB  = BasicBlock::Create(M.getContext(), "for.inc",  LF);
        auto exitBB = BasicBlock::Create(M.getContext(), "for.exit", LF);

        // init
        if (auto *d = dynamic_cast<DeclStmt*>(fs->init.get())){
          auto slot = B.CreateAlloca(i32, nullptr, d->name);
          locals[d->name] = slot;
          auto init = emitExpr(d->init.get());
          B.CreateStore(init, slot);
        } else if (auto *a = dynamic_cast<AssignStmt*>(fs->init.get())) emitAssign(a);

        B.CreateBr(condBB);

        // cond
        B.SetInsertPoint(condBB);
        auto c = emitExpr(fs->cond.get());
        B.CreateCondBr(c, bodyBB, exitBB);

        // body
        B.SetInsertPoint(bodyBB);
        for (auto &s2 : fs->body){
          if (auto *d = dynamic_cast<DeclStmt*>(s2.get())){
            auto slot = B.CreateAlloca(i32, nullptr, d->name);
            locals[d->name] = slot;
            auto init = emitExpr(d->init.get());
            B.CreateStore(init, slot);
          } else if (auto *a = dynamic_cast<AssignStmt*>(s2.get())) emitAssign(a);
          else if (auto *add2 = dynamic_cast<AddAssignStmt*>(s2.get())){
            auto rhs2 = emitExpr(add2->rhs.get());
            auto cur2 = B.CreateLoad(i32, locals.at(add2->name));
            B.CreateStore(B.CreateAdd(cur2, rhs2), locals.at(add2->name));
          } else if (auto *ifs2 = dynamic_cast<IfStmt*>(s2.get())){
            auto cond2 = emitExpr(ifs2->cond.get());
            auto then2 = BasicBlock::Create(M.getContext(), "then2", LF);
            auto cont2 = BasicBlock::Create(M.getContext(), "cont2", LF);
            B.CreateCondBr(cond2, then2, cont2);
            B.SetInsertPoint(then2);
            for (auto &s3 : ifs2->thenStmts){
              if (auto *a2 = dynamic_cast<AssignStmt*>(s3.get())) emitAssign(a2);
            }
            B.CreateBr(cont2);
            B.SetInsertPoint(cont2);
          }
        }
        B.CreateBr(incBB);

        // inc
        B.SetInsertPoint(incBB);
        if (auto *a = dynamic_cast<AssignStmt*>(fs->inc.get())) emitAssign(a);
        B.CreateBr(condBB);

        // continue after loop
        B.SetInsertPoint(exitBB);
      } else if (auto *ret = dynamic_cast<ReturnStmt*>(uptr.get())){
        auto v = emitExpr(ret->val.get());
        B.CreateRet(v);
      }
    }

    // In case function didn't return explicitly
    if (!entry->getTerminator()) B.CreateRet(ConstantInt::get(i32,0));
  }
};

} // namespace

void buildFromAST(llvm::Module &M, const FuncAST &F){
  CG cg(M);
  cg.emit(F);
}
