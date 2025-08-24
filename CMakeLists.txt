cmake_minimum_required(VERSION 3.16)
project(VecOpt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM 18.1.8 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include(HandleLLVMOptions)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_llvm_pass_plugin(VecOpt
  src/VecOpt.cpp
  LINK_COMPONENTS
    Core
    Analysis
    ScalarOpts
    Support
    PassPlugin
)

target_include_directories(VecOpt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
