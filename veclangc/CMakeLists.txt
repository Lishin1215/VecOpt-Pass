cmake_minimum_required(VERSION 3.16)
project(veclangc CXX)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(veclangc
  main.cpp
  codegen.cpp
  codegen_parser.cpp
  preprocessor.cpp
  lexer.h parser.h ast.h
)

llvm_map_components_to_libnames(LLVMLibs
  Core Support IRReader
  Analysis TransformUtils
  ScalarOpts Vectorize IPO
  Target MC CodeGen AsmPrinter
  Object Passes
  native nativecodegen
)

target_include_directories(veclangc PRIVATE ${LLVM_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(veclangc PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(veclangc PRIVATE ${LLVMLibs})
