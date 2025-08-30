#pragma once
#include <string>
#include <memory>
#include <vector>

struct Expr { virtual ~Expr()=default; };
struct NumberExpr: Expr { int64_t v; explicit NumberExpr(int64_t v):v(v){} };
struct VarExpr: Expr { std::string name; explicit VarExpr(std::string n):name(std::move(n)){} };
struct IndexExpr: Expr { std::string base; std::unique_ptr<Expr> idx; IndexExpr(std::string b, std::unique_ptr<Expr> i):base(std::move(b)),idx(std::move(i)){} };
enum class BinOp{Add,Sub,Mul,Div,Mod,LT,LE,GT,GE,EQ,NE};
struct BinExpr: Expr { BinOp op; std::unique_ptr<Expr> a,b; BinExpr(BinOp op,std::unique_ptr<Expr> a,std::unique_ptr<Expr> b):op(op),a(std::move(a)),b(std::move(b)){} };
struct NegExpr: Expr { std::unique_ptr<Expr> x; explicit NegExpr(std::unique_ptr<Expr> x):x(std::move(x)){} };

struct Stmt { virtual ~Stmt()=default; };
struct DeclStmt: Stmt { std::string name; std::unique_ptr<Expr> init; DeclStmt(std::string n,std::unique_ptr<Expr> e):name(std::move(n)),init(std::move(e)){} };
struct AssignStmt: Stmt { std::unique_ptr<Expr> lhs; std::unique_ptr<Expr> rhs; AssignStmt(std::unique_ptr<Expr> l,std::unique_ptr<Expr> r):lhs(std::move(l)),rhs(std::move(r)){} };
struct AddAssignStmt: Stmt { std::string name; std::unique_ptr<Expr> rhs; AddAssignStmt(std::string n,std::unique_ptr<Expr> r):name(std::move(n)),rhs(std::move(r)){} };
struct IfStmt: Stmt { std::unique_ptr<Expr> cond; std::vector<std::unique_ptr<Stmt>> thenStmts; IfStmt(std::unique_ptr<Expr> c,std::vector<std::unique_ptr<Stmt>> t):cond(std::move(c)),thenStmts(std::move(t)){} };
struct ForStmt: Stmt { std::unique_ptr<Stmt> init; std::unique_ptr<Expr> cond; std::unique_ptr<Stmt> inc; std::vector<std::unique_ptr<Stmt>> body; };
struct ReturnStmt: Stmt { std::unique_ptr<Expr> val; explicit ReturnStmt(std::unique_ptr<Expr> v):val(std::move(v)){} };

struct FuncAST {
  std::string name; // "sad"
  std::vector<std::unique_ptr<Stmt>> body;
};
