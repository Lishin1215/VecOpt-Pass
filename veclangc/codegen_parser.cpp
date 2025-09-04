#include "ast.h"
#include "codegen.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <map>

using namespace llvm;

namespace {

struct CodeGenVisitor {
    Module &M;
    IRBuilder<> B;
    std::map<std::string, Value*> namedValues;
    Function* currentFunction = nullptr;

    CodeGenVisitor(Module &M) : M(M), B(M.getContext()) {}

    // --- Expression visitor ---
    Value* visit(Expr* e) {
        if (auto* n = dynamic_cast<NumberExpr*>(e)) {
            return ConstantInt::get(Type::getInt32Ty(M.getContext()), n->v);
        }

        if (auto* v = dynamic_cast<VarExpr*>(e)) {
            Value* V = namedValues[v->name];
            if (!V) throw std::runtime_error("Unknown variable name: " + v->name);
            if (auto* alloc = dyn_cast<AllocaInst>(V)) {
                return B.CreateLoad(alloc->getAllocatedType(), alloc, v->name);
            }
            return V; // Argument*
        }

        if (auto* bin = dynamic_cast<BinExpr*>(e)) {
            Value* L = visit(bin->a.get());
            Value* R = visit(bin->b.get());
            switch (bin->op) {
                case BinOp::Add: return B.CreateAdd(L, R, "addtmp");
                case BinOp::Sub: return B.CreateSub(L, R, "subtmp");
                case BinOp::Mul: return B.CreateMul(L, R, "multmp");
                case BinOp::Div: return B.CreateSDiv(L, R, "divtmp");
                case BinOp::LT:  return B.CreateICmpSLT(L, R, "cmptmp");
                default: throw std::runtime_error("unsupported binary operator");
            }
        }

        if (auto* assign = dynamic_cast<AssignExpr*>(e)) {
            if (auto* var = dynamic_cast<VarExpr*>(assign->lhs.get())) {
                Value* varPtr = namedValues[var->name];
                if (!isa<AllocaInst>(varPtr))
                    throw std::runtime_error("assignment to non-lvalue: " + var->name);
                Value* val = visit(assign->rhs.get());
                B.CreateStore(val, varPtr);
                return val;
            }
            if (auto* idx = dynamic_cast<IndexExpr*>(assign->lhs.get())) {
                Value* basePtr = namedValues[idx->base];
                Value* offset = visit(idx->idx.get());
                Value* addr = B.CreateGEP(Type::getInt32Ty(M.getContext()), basePtr, offset, "ptr_addr");
                Value* val = visit(assign->rhs.get());
                B.CreateStore(val, addr);
                return val;
            }
        }

        // 修正 IndexExpr 處理：
        if (auto* idx = dynamic_cast<IndexExpr*>(e)) {
            Value* basePtr = nullptr;

            // 尋找符號表或是函數參數
            auto it = namedValues.find(idx->base);
            if (it != namedValues.end()) {
                basePtr = it->second;
            } else {
                throw std::runtime_error("Unknown array/pointer name: " + idx->base);
            }

            Value* offset = visit(idx->idx.get());

            // 修正: 兼容不同版本 LLVM 的 PointerType 處理
            Type* elemType = Type::getInt32Ty(M.getContext());
            if (auto* ptrTy = dyn_cast<PointerType>(basePtr->getType())) {
                // 新的 LLVM API 版本用法
                elemType = ptrTy->getNonOpaquePointerElementType();
            }

            Value* addr = B.CreateGEP(elemType, basePtr, offset, idx->base + "_idx");
            return B.CreateLoad(elemType, addr, idx->base + "_load");
        }

        throw std::runtime_error("unknown expression type in codegen");
    }

    // --- Statement visitor ---
    void visit(Stmt* s) {
        if (auto* decl = dynamic_cast<DeclStmt*>(s)) {
            AllocaInst* slot =
                B.CreateAlloca(Type::getInt32Ty(M.getContext()), nullptr, decl->name);
            namedValues[decl->name] = slot;
            if (decl->init) {
                Value* initVal = visit(decl->init.get());
                B.CreateStore(initVal, slot);
            }

        } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(s)) {
            visit(exprStmt->expr.get());

        } else if (auto* ret = dynamic_cast<ReturnStmt*>(s)) {
            B.CreateRet(visit(ret->val.get()));

        } else if (auto* forStmt = dynamic_cast<ForStmt*>(s)) {
            BasicBlock *CondBB  = BasicBlock::Create(M.getContext(), "cond", currentFunction);
            BasicBlock *LoopBB  = BasicBlock::Create(M.getContext(), "loop", currentFunction);
            BasicBlock *AfterBB = BasicBlock::Create(M.getContext(), "afterloop", currentFunction);

            // init
            if (forStmt->init) visit(forStmt->init.get());
            B.CreateBr(CondBB);

            // cond
            B.SetInsertPoint(CondBB);
            Value* CondV = visit(forStmt->cond.get());
            B.CreateCondBr(CondV, LoopBB, AfterBB);

            // body
            B.SetInsertPoint(LoopBB);
            for (auto &stmt : forStmt->body) {
                visit(stmt.get());
            }
            if (forStmt->inc) visit(forStmt->inc.get());
            B.CreateBr(CondBB);

            // after loop
            B.SetInsertPoint(AfterBB);

        } else if (auto* ws = dynamic_cast<WhileStmt*>(s)) {
            BasicBlock *CondBB  = BasicBlock::Create(M.getContext(), "while.cond", currentFunction);
            BasicBlock *LoopBB  = BasicBlock::Create(M.getContext(), "while.body", currentFunction);
            BasicBlock *AfterBB = BasicBlock::Create(M.getContext(), "while.end", currentFunction);

            B.CreateBr(CondBB);

            B.SetInsertPoint(CondBB);
            Value* CondV = visit(ws->cond.get());
            B.CreateCondBr(CondV, LoopBB, AfterBB);

            B.SetInsertPoint(LoopBB);
            for (auto &stmt : ws->body) visit(stmt.get());
            B.CreateBr(CondBB);

            B.SetInsertPoint(AfterBB);
          } else if (auto* block = dynamic_cast<BlockStmt*>(s)) {
          for (auto &stmt : block->stmts) {
              visit(stmt.get());
          }
       } else {
           throw std::runtime_error("unknown statement type in codegen");
       }
   }

    // --- Function visitor ---
    void visit(const FuncAST &F) {
        // 依 F.params 型別建立參數 LLVM 型別
        auto *i32 = Type::getInt32Ty(M.getContext());
        auto *pi32 = PointerType::getUnqual(i32);
        std::vector<Type*> ParamTypes;
        ParamTypes.reserve(F.params.size());
        for (auto &p : F.params) {
            // p.first 是型別字串: "int", "int*", "int**"（此處只需分辨是否含 '*'）
            if (p.first.find('*') != std::string::npos) ParamTypes.push_back(pi32);
            else ParamTypes.push_back(i32);
        }

        FunctionType *FT =
            FunctionType::get(i32, ParamTypes, false);
        currentFunction =
            Function::Create(FT, Function::ExternalLinkage, F.name, &M);

        // 設定參數名稱並加入符號表
        unsigned i = 0;
        for (auto &Arg : currentFunction->args()) {
            Arg.setName(F.params[i].second);
            namedValues[F.params[i].second] = &Arg;
            ++i;
        }

        BasicBlock *entry =
            BasicBlock::Create(M.getContext(), "entry", currentFunction);
        B.SetInsertPoint(entry);

        // 重要：清除並重新填充符號表
        namedValues.clear();

        // 設定參數名稱並加入符號表
        i = 0;
        for (auto &Arg : currentFunction->args()) {
            Arg.setName(F.params[i].second);
            namedValues[std::string(Arg.getName())] = &Arg;
            ++i;
        }

        for (auto& stmt : F.body) {
            visit(stmt.get());
        }

        if (!B.GetInsertBlock()->getTerminator()) {
            B.CreateRet(ConstantInt::get(Type::getInt32Ty(M.getContext()), 0));
        }

        verifyFunction(*currentFunction);
    }
};

} // namespace

void buildFromAST(llvm::Module &M, const FuncAST &F) {
    CodeGenVisitor visitor(M);
    visitor.visit(F);
}
