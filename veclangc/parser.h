#pragma once
#include "lexer.h"
#include "ast.h"
#include <memory>
#include <stdexcept>

class Parser {
  Lexer L;
  Token tok;
  void bump(){ tok = L.next(); }
  bool is(Tok k) const { return tok.kind==k; }
  void expect(Tok k, const char* msg){ if(!is(k)) throw std::runtime_error(msg); bump(); }

  std::unique_ptr<Expr> parsePrimary(){
    if (is(Tok::Number)){ auto v=tok.num; bump(); return std::make_unique<NumberExpr>(v); }
    if (is(Tok::Ident)){
      std::string name = tok.text; bump();
      if (is(Tok::LBracket)){ bump(); auto e = parseExpr(); expect(Tok::RBracket, "] expected");
        return std::make_unique<IndexExpr>(name,std::move(e)); }
      return std::make_unique<VarExpr>(name);
    }
    if (is(Tok::LParen)){ bump(); auto e=parseExpr(); expect(Tok::RParen, ") expected"); return e; }
    if (is(Tok::Minus)){ bump(); auto e=parsePrimary(); return std::make_unique<NegExpr>(std::move(e)); }
    throw std::runtime_error("bad primary");
  }
  int prec(Tok k){
    switch(k){
      case Tok::Mul: case Tok::Div: case Tok::Mod: return 3;
      case Tok::Plus: case Tok::Minus: return 2;
      case Tok::Lt: case Tok::Le: case Tok::Gt: case Tok::Ge: return 1;
      case Tok::EqEq: case Tok::Ne: return 1;
      default: return -1;
    }
  }
  BinOp toOp(Tok k){
    switch(k){
      case Tok::Plus: return BinOp::Add; case Tok::Minus: return BinOp::Sub;
      case Tok::Mul: return BinOp::Mul; case Tok::Div: return BinOp::Div; case Tok::Mod: return BinOp::Mod;
      case Tok::Lt:  return BinOp::LT;  case Tok::Le:    return BinOp::LE;
      case Tok::Gt:  return BinOp::GT;  case Tok::Ge:    return BinOp::GE;
      case Tok::EqEq:return BinOp::EQ;  case Tok::Ne:    return BinOp::NE;
      default: throw std::runtime_error("bad binop");
    }
  }
  std::unique_ptr<Expr> parseBinRHS(int minPrec, std::unique_ptr<Expr> lhs){
    while(true){
      int p = prec(tok.kind); if (p < minPrec) return lhs;
      Tok opTok = tok.kind; bump();
      auto rhs = parsePrimary();
      int p2 = prec(tok.kind);
      if (p2 > p){ rhs = parseBinRHS(p+1, std::move(rhs)); }
      lhs = std::make_unique<BinExpr>(toOp(opTok), std::move(lhs), std::move(rhs));
    }
  }
  std::unique_ptr<Expr> parseExpr(){ auto lhs = parsePrimary(); return parseBinRHS(0,std::move(lhs)); }

  std::unique_ptr<Stmt> parseDeclOrAssign(){
    if (is(Tok::KwInt)){ // int x = ...
      bump();
      if (is(Tok::KwConst)) bump(); // ignore const
      while(is(Tok::Star)) bump();  // support pointers syntax
      if (!is(Tok::Ident)) throw std::runtime_error("var name expected");
      std::string name = tok.text; bump();
      expect(Tok::Assign, "= expected");
      auto e = parseExpr();
      expect(Tok::Semicolon, "; expected");
      return std::make_unique<DeclStmt>(name, std::move(e));
    }
    if (is(Tok::Ident)){
      std::string name = tok.text; bump();
      if (is(Tok::Plus)){ bump(); expect(Tok::Assign, "+= expected");
        auto e = parseExpr(); expect(Tok::Semicolon,"; expected");
        return std::make_unique<AddAssignStmt>(name, std::move(e));
      }
      std::unique_ptr<Expr> lhs;
      if (is(Tok::LBracket)){ bump(); auto idx = parseExpr(); expect(Tok::RBracket, "] expected");
        lhs = std::make_unique<IndexExpr>(name, std::move(idx)); }
      else lhs = std::make_unique<VarExpr>(name);
      expect(Tok::Assign, "= expected");
      auto rhs = parseExpr(); expect(Tok::Semicolon,"; expected");
      return std::make_unique<AssignStmt>(std::move(lhs), std::move(rhs));
    }
    throw std::runtime_error("unsupported statement");
  }

  std::unique_ptr<Stmt> parseIf(){
    expect(Tok::KwIf,"if");
    expect(Tok::LParen,"(");
    auto cond = parseExpr();
    expect(Tok::RParen,")");
    std::vector<std::unique_ptr<Stmt>> thenS;
    if (is(Tok::LBrace)){
      bump();
      while(!is(Tok::RBrace)) thenS.push_back(parseStmt());
      expect(Tok::RBrace,"}");
    }else{
      thenS.push_back(parseStmt());
    }
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenS));
  }

  std::unique_ptr<Stmt> parseFor(){
    expect(Tok::KwFor,"for");
    expect(Tok::LParen,"(");
    auto init = parseDeclOrAssign();
    auto cond = parseExpr(); expect(Tok::Semicolon,";");
    std::unique_ptr<Stmt> inc;
    if (is(Tok::Ident)){
      std::string name = tok.text; bump();
      if (is(Tok::PlusPlus)){ bump();
        inc = std::make_unique<AssignStmt>(std::make_unique<VarExpr>(name),
               std::make_unique<BinExpr>(BinOp::Add,std::make_unique<VarExpr>(name),
               std::make_unique<NumberExpr>(1)));
      } else {
        expect(Tok::Assign,"=");
        auto e = parseExpr();
        inc = std::make_unique<AssignStmt>(std::make_unique<VarExpr>(name), std::move(e));
      }
    } else throw std::runtime_error("for-inc expected");
    expect(Tok::RParen,")");

    std::vector<std::unique_ptr<Stmt>> body;
    expect(Tok::LBrace,"{");
    while(!is(Tok::RBrace)) body.push_back(parseStmt());
    expect(Tok::RBrace,"}");

    auto fs = std::make_unique<ForStmt>();
    fs->init = std::move(init); fs->cond = std::move(cond); fs->inc = std::move(inc); fs->body = std::move(body);
    return fs;
  }

  std::unique_ptr<Stmt> parseReturn(){
    expect(Tok::KwReturn,"return");
    auto e = parseExpr();
    expect(Tok::Semicolon,";");
    return std::make_unique<ReturnStmt>(std::move(e));
  }

  std::unique_ptr<Stmt> parseStmt(){
    if (is(Tok::KwInt)) return parseDeclOrAssign();
    if (is(Tok::KwIf))  return parseIf();
    if (is(Tok::KwFor)) return parseFor();
    if (is(Tok::KwReturn)) return parseReturn();
    if (is(Tok::Ident)) return parseDeclOrAssign();
    throw std::runtime_error("unsupported statement");
  }

public:
  explicit Parser(std::string s): L(std::move(s)) { bump(); }

  FuncAST parseFunction() {
    // Small helper to parse exactly: int sad(const int* a, const int* b, int n)
    auto parseSignature = [&]() {
        expect(Tok::KwInt,"int");
        if (!is(Tok::Ident) || tok.text!="sad") throw std::runtime_error("only supports int sad(...)");
        bump();
        expect(Tok::LParen,"(");
        auto parseParam=[&](){
        if (is(Tok::KwConst)) bump();
        expect(Tok::KwInt,"int");
        while(is(Tok::Star)) bump(); // allow pointers
        if (!is(Tok::Ident)) throw std::runtime_error("param name");
        bump();
        };
        parseParam(); expect(Tok::Comma,",");
        parseParam(); expect(Tok::Comma,",");
        expect(Tok::KwInt,"int"); if (!is(Tok::Ident)) throw std::runtime_error("n name"); bump();
        expect(Tok::RParen,")");
    };

    // First, try to parse signature
    parseSignature();

    // If this is only a prototype (ends with ';'), skip it and parse the real definition next.
    if (is(Tok::Semicolon)) {
        bump();            // consume ';'
        parseSignature();  // parse the same signature again for the definition
    }

    // Now we require a function body
    expect(Tok::LBrace,"{");
    std::vector<std::unique_ptr<Stmt>> body;
    while(!is(Tok::RBrace)) body.push_back(parseStmt());
    expect(Tok::RBrace,"}");

    FuncAST F; F.name="sad"; F.body = std::move(body); return F;
    }

};
