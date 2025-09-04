cmake_minimum_required(VERSION 3.16)
project(hybrid C)

# add_custom_command(
#   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sad_kernel.o
#   COMMAND veclangc --input ${CMAKE_CURRENT_SOURCE_DIR}/sad_kernel.c -I ${CMAKE_CURRENT_SOURCE_DIR} -c -o sad_kernel.o
#   DEPENDS veclangc ${CMAKE_CURRENT_SOURCE_DIR}/sad_kernel.c ${CMAKE_CURRENT_SOURCE_DIR}/include/sad.h
#   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
#   VERBATIM)



# add_custom_target(sad_obj DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/sad_kernel.o)


# include_directories(${CMAKE_CURRENT_SOURCE_DIR})
# add_executable(app driver.c ${CMAKE_CURRENT_BINARY_DIR}/sad_kernel.o)


# add_dependencies(app sad_obj)


add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sad_kernel.o
  COMMAND veclangc --input ${CMAKE_CURRENT_SOURCE_DIR}/sad_kernel.c
                   -I ${CMAKE_CURRENT_SOURCE_DIR}
                   -c -o sad_kernel.o
  DEPENDS veclangc
          ${CMAKE_CURRENT_SOURCE_DIR}/sad_kernel.c
          ${CMAKE_CURRENT_SOURCE_DIR}/include/sad.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  VERBATIM)

add_custom_target(sad_obj DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/sad_kernel.o)

add_executable(app driver.c ${CMAKE_CURRENT_BINARY_DIR}/sad_kernel.o)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
