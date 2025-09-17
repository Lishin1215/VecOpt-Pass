#pragma once
#include "lexer.h"
#include "ast.h"
#include <memory>
#include <stdexcept>

class Parser {
    Lexer L;
    Token tok;

    void bump() { tok = L.next(); }
    bool is(Tok k) const { return tok.kind == k; }
    void expect(Tok k, const char *msg) {
        if (!is(k)) throw std::runtime_error(msg);
        bump();
    }

    // --- Expression Parsing ---
    std::unique_ptr<Expr> parsePrimary() {
        if (is(Tok::Number)) {
            auto v = tok.num;
            bump();
            return std::make_unique<NumberExpr>(v);
        }
        if (is(Tok::Ident)) {
            std::string name = tok.text;
            bump();
            if (is(Tok::LParen)) { // function call
                bump();
                std::vector<std::unique_ptr<Expr>> args;
                if (!is(Tok::RParen)) {
                    do {
                        args.push_back(parseExpr());
                    } while (is(Tok::Comma) && (bump(), true));
                }
                expect(Tok::RParen, ") expected after function arguments");
                return std::make_unique<CallExpr>(name, std::move(args));
            }
            if (is(Tok::LBracket)) { // array indexing
                bump();
                auto e = parseExpr();
                expect(Tok::RBracket, "] expected");
                return std::make_unique<IndexExpr>(name, std::move(e));
            }
            return std::make_unique<VarExpr>(name);
        }
        if (is(Tok::LParen)) {
            bump();
            auto e = parseExpr();
            expect(Tok::RParen, ") expected");
            return e;
        }
        throw std::runtime_error("unexpected token in expression");
    }

    int prec(Tok k) {
        switch (k) {
            case Tok::Mul: case Tok::Div:                return 70;
            case Tok::Plus: case Tok::Minus:             return 60;
            case Tok::Shl:  case Tok::Shr:               return 50;
            case Tok::Lt: case Tok::Gt: case Tok::Le: case Tok::Ge: return 40;
            case Tok::EqEq: case Tok::Ne:                return 30;
            case Tok::Amp:                               return 20; // &
            case Tok::Caret:                             return 15; // ^
            case Tok::Pipe:                              return 10; // |
        default: return -1;
        }
    }

    BinOp toOp(Tok k) {
        switch (k) {
        case Tok::Plus: return BinOp::Add; case Tok::Minus: return BinOp::Sub;
        case Tok::Mul: return BinOp::Mul; case Tok::Div: return BinOp::Div;
        case Tok::Lt: return BinOp::LT;   case Tok::Le: return BinOp::LE;
        case Tok::Gt: return BinOp::GT;   case Tok::Ge: return BinOp::GE;
        case Tok::EqEq: return BinOp::EQ; case Tok::Ne: return BinOp::NE;
        case Tok::Amp:   return BinOp::And;
        case Tok::Pipe:  return BinOp::Or;
        case Tok::Caret: return BinOp::Xor;
        case Tok::Shl:   return BinOp::Shl;
        case Tok::Shr:   return BinOp::Shr;
        default: throw std::runtime_error("bad binary operator");
        }
    }

    std::unique_ptr<Expr> parseBinRHS(int minPrec, std::unique_ptr<Expr> lhs) {
        while (true) {
            int p = prec(tok.kind);
            if (p < minPrec) return lhs;
            Tok opTok = tok.kind;
            bump();
            auto rhs = parsePrimary();
            int p2 = prec(tok.kind);
            if (p2 > p) rhs = parseBinRHS(p + 1, std::move(rhs));
            lhs = std::make_unique<BinExpr>(toOp(opTok), std::move(lhs), std::move(rhs));
        }
    }

    std::unique_ptr<Expr> parseAssignment() {
        auto lhs = parseBinRHS(0, parsePrimary());
        if (is(Tok::Assign)) {
            bump();
            auto rhs = parseAssignment();
            if (!dynamic_cast<VarExpr*>(lhs.get()) &&
                !dynamic_cast<IndexExpr*>(lhs.get())) {
                throw std::runtime_error("invalid assignment target");
            }
            return std::make_unique<AssignExpr>(std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    // --- Statement Parsing ---
    std::unique_ptr<Stmt> parseDeclaration(bool expectSemi) {
        expect(Tok::KwInt, "int");
        if (!is(Tok::Ident)) throw std::runtime_error("variable name expected");
        std::string name = tok.text;
        bump();
        std::unique_ptr<Expr> init;
        if (is(Tok::Assign)) {
            bump();
            init = parseExpr();
        }
        if (expectSemi) expect(Tok::Semicolon, "; expected");
        return std::make_unique<DeclStmt>(name, std::move(init));
    }

    std::unique_ptr<Stmt> parseIf() {
        expect(Tok::KwIf, "if");
        expect(Tok::LParen, "(");
        auto cond = parseExpr();
        expect(Tok::RParen, ")");
        std::vector<std::unique_ptr<Stmt>> thenStmts;
        thenStmts.push_back(parseStmt());

        // else?
        if (is(Tok::KwElse)) {
            bump();
            // Simplified: else body is treated as another IfStmt with const true
            std::vector<std::unique_ptr<Stmt>> elseStmts;
            elseStmts.push_back(parseStmt());
            // TODO: AST should add else branch, currently ignored here
        }
        return std::make_unique<IfStmt>(std::move(cond), std::move(thenStmts));
    }

    std::unique_ptr<Stmt> parseFor() {
        expect(Tok::KwFor, "for");
        expect(Tok::LParen, "(");
        std::unique_ptr<Stmt> init;
        if (!is(Tok::Semicolon)) {
            if (is(Tok::KwInt)) init = parseDeclaration(false);
            else init = std::make_unique<ExprStmt>(parseExpr());
        }
        expect(Tok::Semicolon, "; after for-init");
        std::unique_ptr<Expr> cond;
        if (!is(Tok::Semicolon)) cond = parseExpr();
        expect(Tok::Semicolon, "; after for-cond");
        std::unique_ptr<Expr> inc;
        if (!is(Tok::RParen)) inc = parseExpr();
        expect(Tok::RParen, ")");
        std::vector<std::unique_ptr<Stmt>> body;
        if (is(Tok::LBrace)) {
            bump();
            while (!is(Tok::RBrace)) body.push_back(parseStmt());
            expect(Tok::RBrace, "} for-body");
        } else {
            body.push_back(parseStmt());
        }
        return std::make_unique<ForStmt>(
            std::move(init), std::move(cond), std::move(inc), std::move(body));
    }

std::unique_ptr<Stmt> parseBlock() {
    expect(Tok::LBrace, "{");
    std::vector<std::unique_ptr<Stmt>> body;
    while (!is(Tok::RBrace)) {
        body.push_back(parseStmt());
    }
    expect(Tok::RBrace, "}");
    return std::make_unique<BlockStmt>(std::move(body));
}


    std::unique_ptr<Stmt> parseWhile() {
    expect(Tok::KwWhile, "while");
    expect(Tok::LParen, "(");
    auto cond = parseExpr();
    expect(Tok::RParen, ")");
    std::vector<std::unique_ptr<Stmt>> body;
    if (is(Tok::LBrace)) {
        bump();
        while (!is(Tok::RBrace)) body.push_back(parseStmt());
        expect(Tok::RBrace, "}");
    } else {
        body.push_back(parseStmt());
    }
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
}



public:
    explicit Parser(std::string s) : L(std::move(s)) { bump(); }

    std::unique_ptr<Expr> parseExpr() { return parseAssignment(); }

    std::unique_ptr<Stmt> parseStmt() {
        if (is(Tok::KwInt)) return parseDeclaration(true);
        if (is(Tok::KwFor)) return parseFor();
        if (is(Tok::KwIf)) return parseIf();
        if (is(Tok::KwWhile)) return parseWhile();
        if (is(Tok::KwReturn)) {
            bump();
            auto val = parseExpr();
            expect(Tok::Semicolon, "; expected after return");
            return std::make_unique<ReturnStmt>(std::move(val));
        }
        if (is(Tok::LBrace)) return parseBlock();

        // fallback: expression stmt
        auto expr = parseExpr();
        if (!is(Tok::Semicolon)) {
            throw std::runtime_error("; expected after expression, found: " + std::to_string(static_cast<int>(tok.kind)));
        }
        bump(); // consume the semicolon
        return std::make_unique<ExprStmt>(std::move(expr));
    }

    FuncAST parseFunction() {
        // return type (only 'int' supported)
        expect(Tok::KwInt, "int");
        if (!is(Tok::Ident)) throw std::runtime_error("function name expected");
        std::string fname = tok.text;
        bump();
        expect(Tok::LParen, "(");

        // parse params: forms like "int x" or "int *xs"
        std::vector<std::pair<std::string,std::string>> params;
        if (!is(Tok::RParen)) {
            while (true) {
                // type
                expect(Tok::KwInt, "int");
                std::string ty = "int";
                // while (is(Tok::Star)) { bump(); ty += "*"; }
                while (is(Tok::Star) || is(Tok::Mul)) { bump(); ty += "*"; }
                // name
                if (!is(Tok::Ident)) throw std::runtime_error("param name expected");
                std::string pname = tok.text; bump();
                params.emplace_back(ty, pname);
                if (!is(Tok::Comma)) break;
                bump(); // consume comma
            }
        }
        expect(Tok::RParen, ")");

        expect(Tok::LBrace, "{");
        std::vector<std::unique_ptr<Stmt>> body;
        while (!is(Tok::RBrace)) body.push_back(parseStmt());
        expect(Tok::RBrace, "}");

        FuncAST F;
        F.name = fname;
        F.params = std::move(params);
        F.body = std::move(body);
        return F;
    }
};
