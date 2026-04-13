cmake_minimum_required(VERSION 3.20)

project(gate-lang
  VERSION 0.1.0
  DESCRIPTION "Hardware description language: parse, compile to DAG, simulate"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Handy for clang-tidy / IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- cpp-peglib (header-only; provides target `peglib`) ---
include(FetchContent)

# Respect peglib's options without pulling tests, examples, or extra tools
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PEGLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PEGLIB_BUILD_LINT OFF CACHE BOOL "" FORCE)
set(PEGLIB_BUILD_PL0 OFF CACHE BOOL "" FORCE)
set(PEGLIB_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  cpp-peglib
  GIT_REPOSITORY https://github.com/yhirose/cpp-peglib.git
  GIT_TAG        v1.10.1
)
FetchContent_MakeAvailable(cpp-peglib)

# --- gate-lang library (everything except the CLI entry point) ---
add_library(gate_lang STATIC
  src/compiler/Compiler.cpp
  src/parser/Parser.cpp
  src/simulation/Simulation.cpp
  src/ui/OutputFormatter.cpp
  src/ui/Repl.cpp
)

target_include_directories(gate_lang
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# PUBLIC: headers under include/ may include <peglib.h>
target_link_libraries(gate_lang PUBLIC peglib)

# --- CLI ---
add_executable(gate-lang-cli src/main.cpp)
set_target_properties(gate-lang-cli PROPERTIES
  OUTPUT_NAME gate-lang
)
target_link_libraries(gate-lang-cli PRIVATE gate_lang)
