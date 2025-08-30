#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// A tiny preprocessor supporting:
//  - #include "file.h"  (relative and user include dirs)
//  - #define NAME value (no-parameter macros)
//  - skip #line markers (# 1 "file")
//  - ignore any other #... lines
// Limitations: no conditional compilation, no function-like macros, no system <...> includes.

class Preprocessor {
public:
  // Add include search directory (searched after the including file's directory).
  void addIncludeDir(std::string dir);

  // Preprocess a .c file and return a flat C source string.
  // Throws std::runtime_error on IO errors.
  std::string run(const std::string& path);

private:
  std::vector<std::string> includeDirs_;
  std::unordered_map<std::string, std::string> macros_;   // NAME -> replacement
  std::unordered_set<std::string> includeStack_;           // to avoid recursive include loops

  std::string processFile(const std::string& fullPath);
  static std::string dirName(const std::string& path);
  static std::string joinPath(const std::string& a, const std::string& b);
  static bool startsWith(const std::string& s, const char* pfx);
  static std::string trim(const std::string& s);

  // Expand simple object-like macros on a line, avoiding string literals.
  std::string expandMacrosLine(const std::string& line);
};
