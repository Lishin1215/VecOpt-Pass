#pragma once
#include <string>
#include <memory>
#include <vector>

// --- Expressions (運算式) ---
struct Expr { virtual ~Expr() = default; };

struct NumberExpr : Expr {
    int64_t v;
    explicit NumberExpr(int64_t v) : v(v) {}
};

struct VarExpr : Expr {
    std::string name;
    explicit VarExpr(std::string n) : name(std::move(n)) {}
};

struct IndexExpr : Expr {
    std::string base;
    std::unique_ptr<Expr> idx;
    IndexExpr(std::string b, std::unique_ptr<Expr> i)
        : base(std::move(b)), idx(std::move(i)) {}
};

enum class BinOp { Add, Sub, Mul, Div, Mod, LT, LE, GT, GE, EQ, NE };

struct BinExpr : Expr {
    BinOp op;
    std::unique_ptr<Expr> a, b;
    BinExpr(BinOp op, std::unique_ptr<Expr> a, std::unique_ptr<Expr> b)
        : op(op), a(std::move(a)), b(std::move(b)) {}
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr(std::string c, std::vector<std::unique_ptr<Expr>> a)
        : callee(std::move(c)), args(std::move(a)) {}
};

struct AssignExpr : Expr {
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
    AssignExpr(std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : lhs(std::move(l)), rhs(std::move(r)) {}
};

// --- Statements (語句) ---
struct Stmt { virtual ~Stmt() = default; };

struct DeclStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> init; // Can be nullptr
    DeclStmt(std::string n, std::unique_ptr<Expr> e)
        : name(std::move(n)), init(std::move(e)) {}
};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
    explicit ExprStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> cond;
    std::vector<std::unique_ptr<Stmt>> thenStmts;
    IfStmt(std::unique_ptr<Expr> c, std::vector<std::unique_ptr<Stmt>> t)
        : cond(std::move(c)), thenStmts(std::move(t)) {}
};

struct ForStmt : Stmt {
    std::unique_ptr<Stmt> init;
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Expr> inc;
    std::vector<std::unique_ptr<Stmt>> body;
    ForStmt(std::unique_ptr<Stmt> i,
            std::unique_ptr<Expr> c,
            std::unique_ptr<Expr> n,
            std::vector<std::unique_ptr<Stmt>> b)
        : init(std::move(i)), cond(std::move(c)),
          inc(std::move(n)), body(std::move(b)) {}
};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> val;
    explicit ReturnStmt(std::unique_ptr<Expr> v) : val(std::move(v)) {}
};

struct BlockStmt : Stmt {
    std::vector<std::unique_ptr<Stmt>> stmts;
    explicit BlockStmt(std::vector<std::unique_ptr<Stmt>> s) : stmts(std::move(s)) {}
};

struct WhileStmt : Stmt {
    std::unique_ptr<Expr> cond;
    std::vector<std::unique_ptr<Stmt>> body;
    WhileStmt(std::unique_ptr<Expr> c, std::vector<std::unique_ptr<Stmt>> b)
        : cond(std::move(c)), body(std::move(b)) {}
};

// --- Top Level ---
struct FuncAST {
    std::string name;
    std::vector<std::pair<std::string, std::string>> params; 
    std::vector<std::unique_ptr<Stmt>> body;
};
