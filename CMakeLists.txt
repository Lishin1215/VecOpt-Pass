cmake_minimum_required(VERSION 3.16)
project(VecOpt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Find LLVM (no overly strict patch-level requirement)
find_package(LLVM 18 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
include(HandleLLVMOptions)

# Avoid global add_definitions; prefer target-level include later
# include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})  # not needed

add_llvm_pass_plugin(VecOpt
  src/VecOpt.cpp
  LINK_COMPONENTS
    Core
    Analysis
    ScalarOpts
    Support
    PassPlugin
)

# prefer target-level include dirs; keep project includes
target_include_directories(VecOpt PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${LLVM_INCLUDE_DIRS}
)

# If you need to link specific LLVM libraries manually, use:
# llvm_map_components_to_libnames(REQ_LLVM_LIBS support core ...)
# target_link_libraries(VecOpt PRIVATE ${REQ_LLVM_LIBS})

add_subdirectory(veclangc)
add_subdirectory(hybrid)