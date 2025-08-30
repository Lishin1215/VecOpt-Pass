#include "preprocessor.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <sys/stat.h>

static bool fileExists(const std::string& p){
  struct stat st{}; return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

void Preprocessor::addIncludeDir(std::string dir){
  if (!dir.empty() && dir.back()=='/') dir.pop_back();
  includeDirs_.push_back(std::move(dir));
}

std::string Preprocessor::dirName(const std::string& p){
  auto pos = p.find_last_of("/\\");
  if (pos == std::string::npos) return ".";
  return p.substr(0, pos);
}

std::string Preprocessor::joinPath(const std::string& a, const std::string& b){
  if (a.empty()) return b;
  if (!a.empty() && (a.back()=='/' || a.back()=='\\')) return a + b;
  return a + "/" + b;
}

bool Preprocessor::startsWith(const std::string& s, const char* pfx){
  const size_t n = std::char_traits<char>::length(pfx);
  return s.size() >= n && s.compare(0, n, pfx) == 0;
}

std::string Preprocessor::trim(const std::string& s){
  size_t i=0, j=s.size();
  while (i<j && std::isspace((unsigned char)s[i])) ++i;
  while (j>i && std::isspace((unsigned char)s[j-1])) --j;
  return s.substr(i, j-i);
}

std::string Preprocessor::expandMacrosLine(const std::string& line){
  // Token-wise replace macros, but do not touch inside string/char literals.
  std::string out;
  out.reserve(line.size());
  bool inSQ=false, inDQ=false, escape=false;

  auto flushToken = [&](const std::string& tok){
    auto it = macros_.find(tok);
    if (it != macros_.end()) out += it->second;
    else out += tok;
  };

  std::string tok;

  for (size_t i=0;i<line.size();++i){
    char c = line[i];
    if (inSQ || inDQ){
      out.push_back(c);
      if (escape){ escape=false; continue; }
      if (c=='\\'){ escape=true; continue; }
      if (inSQ && c=='\'') inSQ=false;
      if (inDQ && c=='"')  inDQ=false;
      continue;
    }
    if (c=='\''){ // entering char literal
      if (!tok.empty()){ flushToken(tok); tok.clear(); }
      inSQ = true; out.push_back(c); continue;
    }
    if (c=='"'){ // entering string literal
      if (!tok.empty()){ flushToken(tok); tok.clear(); }
      inDQ = true; out.push_back(c); continue;
    }
    auto isIdChar = [&](char ch)->bool{ return std::isalnum((unsigned char)ch) || ch=='_'; };
    if (isIdChar(c)){
      tok.push_back(c);
    } else {
      if (!tok.empty()){ flushToken(tok); tok.clear(); }
      out.push_back(c);
    }
  }
  if (!tok.empty()) flushToken(tok);
  return out;
}

std::string Preprocessor::processFile(const std::string& fullPath){
  if (includeStack_.count(fullPath)) {
    throw std::runtime_error("include cycle detected: " + fullPath);
  }
  includeStack_.insert(fullPath);

  std::ifstream ifs(fullPath);
  if (!ifs) throw std::runtime_error("cannot open: " + fullPath);

  const std::string selfDir = dirName(fullPath);
  std::ostringstream out;

  std::string line;
  while (std::getline(ifs, line)){
    std::string s = trim(line);
    if (!s.empty() && s[0] == '#'){
      // skip line markers like: # 1 "file"
      if (s.size()>=2 && (std::isdigit((unsigned char)s[1]) || s[1]=='l')) {
        // '# 1 "..."' or '#line ...' -> skip
        continue;
      }
      if (startsWith(s, "#ifdef")) {
        // parse symbol after #ifdef
        std::istringstream iss(s);
        std::string sharp_ifdef, sym;
        iss >> sharp_ifdef >> sym; // "#ifdef" and then symbol
        if (sym == "__cplusplus") {
          // We are compiling C, so __cplusplus is undefined -> skip until matching #endif.
          int depth = 1;
          std::string l2;
          while (depth > 0 && std::getline(ifs, l2)) {
            std::string t = trim(l2);
            if (!t.empty() && t[0]=='#') {
              if (startsWith(t, "#ifdef")) depth++;
              else if (startsWith(t, "#endif")) depth--;
              // We intentionally ignore #else here (skip whole block).
            }
          }
          continue; // block skipped
        }
        // If it's some other #ifdef, ignore directive line and keep reading normally.
        // (Basic mode: we neither include nor exclude; good enough for our current headers.)
        continue;
      }

      // === NEW: trivial handling of '#endif' that might appear alone ===
      if (startsWith(s, "#endif")) {
        // In our tiny mode, we've either consumed it in the block-skip above,
        // or we just ignore stray #endif lines.
        continue;
      }
      
      // #include "file.h"
      if (startsWith(s, "#include")){
        // Only support quotes includes
        auto q1 = s.find('\"');
        auto q2 = (q1==std::string::npos) ? std::string::npos : s.find('\"', q1+1);
        if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1+1){
          std::string hdr = s.substr(q1+1, q2-q1-1);

          // search order: relative to current file dir, then user include dirs
          std::string cand = joinPath(selfDir, hdr);
          if (!fileExists(cand)){
            for (auto &d : includeDirs_){
              auto c2 = joinPath(d, hdr);
              if (fileExists(c2)){ cand = c2; break; }
            }
          }
          if (!fileExists(cand)) {
            // Ignore system-style <...> includes or missing files silently
            continue;
          }

          out << processFile(cand); // recursive include
          continue;
        }
        // unsupported include form -> ignore
        continue;
      }
      // #define NAME value   (no-parameter)
      if (startsWith(s, "#define")){
        // tokenize "#define NAME value..."
        std::istringstream iss(s);
        std::string sharp, kw, name;
        iss >> sharp >> name; // sharp should be "#define"
        if (name.empty()) continue;
        std::string rest;
        std::getline(iss, rest);
        rest = trim(rest);
        if (startsWith(rest.c_str(), "(")) {
          // function-like macro: ignore in tiny mode
          continue;
        }
        macros_[name] = rest;
        continue;
      }
      // any other directive: ignore
      continue;
    }

    // normal line: expand macros
    out << expandMacrosLine(line) << "\n";
  }

  includeStack_.erase(fullPath);
  return out.str();
}

std::string Preprocessor::run(const std::string& path){
  // Reset per-run state except includeDirs_
  macros_.clear();
  includeStack_.clear();
  return processFile(path);
}
