#pragma once
#include <string>
#include <cctype>

enum class Tok {
  Eof, Ident, Number,
  KwInt, KwConst, KwReturn, KwFor, KwIf, KwElse, KwWhile,
  Star, Amp, LParen, RParen, LBrace, RBrace, LBracket, RBracket,
  Comma, Semicolon, Assign,
  Plus, Minus, Mul, Div, Mod,
  Lt, Gt, Le, Ge, EqEq, Ne,
  Shl, Shr, Pipe, Caret,
  PlusPlus
};

struct Token {
  Tok kind;
  std::string text;
  int64_t num = 0;
};

class Lexer {
  const std::string src;
  size_t i = 0;
public:
  explicit Lexer(std::string s): src(std::move(s)) {}
  Token next() {
    // Skip preprocessor line markers starting with '#'
    if (i < src.size() && src[i] == '#') {
    while (i < src.size() && src[i] != '\n') ++i; // skip until end of line
    return next(); // restart lexing after skipping the line
    }
    auto skipSpace = [&]{
      while (i < src.size()) {
        if (isspace((unsigned char)src[i])) { ++i; continue; }
        if (src[i]=='/' && i+1<src.size() && src[i+1]=='/') { while(i<src.size()&&src[i]!='\n') ++i; continue; }
        if (src[i]=='/' && i+1<src.size() && src[i+1]=='*') { i+=2; while(i+1<src.size() && !(src[i]=='*'&&src[i+1]=='/')) ++i; if(i+1<src.size()) i+=2; continue; }
        break;
      }
    };
    skipSpace();
    if (i >= src.size()) return {Tok::Eof,""};
    char c = src[i];

    if (isalpha((unsigned char)c) || c=='_') {
      size_t j=i; while(j<src.size() && (isalnum((unsigned char)src[j]) || src[j]=='_')) ++j;
      std::string w = src.substr(i, j-i); i=j;
      if (w=="int") return {Tok::KwInt,w};
      if (w=="const") return {Tok::KwConst,w};
      if (w=="return") return {Tok::KwReturn,w};
      if (w=="for") return {Tok::KwFor,w};
      if (w=="if") return {Tok::KwIf,w};
      if (w=="else") return {Tok::KwElse,w};
      if (w=="while")   return {Tok::KwWhile, w};
      return {Tok::Ident,w};
    }
    if (isdigit((unsigned char)c)) {
      int64_t v=0; size_t j=i;
      while(j<src.size() && isdigit((unsigned char)src[j])) { v = v*10 + (src[j]-'0'); ++j; }
      i=j; return {Tok::Number,"",v};
    }
    auto two = [&](char a,char b)->int{
      if (i+1<src.size() && src[i]==a && src[i+1]==b){ i+=2; return 1; } return 0; };
    if (two('=','=')) return {Tok::EqEq,"=="};
    if (two('!','=')) return {Tok::Ne,"!="};
    if (two('<','=')) return {Tok::Le,"<="};
    if (two('>','=')) return {Tok::Ge,">="};
    if (two('+','+')) return {Tok::PlusPlus,"++"};

    ++i;
    switch(c){
      // case '*': return {Tok::Star,"*"};
      case '&': return {Tok::Amp,"&"};
      case '(': return {Tok::LParen,"("};
      case ')': return {Tok::RParen,")"};
      case '{': return {Tok::LBrace,"{"};
      case '}': return {Tok::RBrace,"}"};
      case '[': return {Tok::LBracket,"["};
      case ']': return {Tok::RBracket,"]"};
      case ',': return {Tok::Comma,","};
      case ';': return {Tok::Semicolon,";"};
      case '=': return {Tok::Assign,"="};
      case '+': return {Tok::Plus,"+"};
      case '-': return {Tok::Minus,"-"};
      case '/': return {Tok::Div,"/"};
      case '%': return {Tok::Mod,"%"}; 
      case '<': return {Tok::Lt,"<"};
      case '>': return {Tok::Gt,">"};
      case '*': return {Tok::Mul, "*"};
      default:  return {Tok::Eof,""};
    }
  }
};
