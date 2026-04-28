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

# --- gateo-cpp (.gateo protobuf I/O + gateo::v3::view types; target gateo::gateo) ---
# Pinned to https://github.com/Rhodes-Gate-Lang/gateo-cpp/releases/tag/v3.0.0
# Pulls gateo-schema v3.0.0 at configure time. Requires protoc + libprotobuf, or
# -DGATEO_FETCH_PROTOBUF=ON.
set(GATEO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  gateo-cpp
  GIT_REPOSITORY https://github.com/Rhodes-Gate-Lang/gateo-cpp.git
  GIT_TAG        v3.0.0
)
FetchContent_MakeAvailable(gateo-cpp)

# --- gate-lang library (everything except the CLI entry point) ---
add_library(gate_lang STATIC
  src/compiler/CompCache.cpp
  src/compiler/CompileComponent.cpp
  src/compiler/CompileExpr.cpp
  src/compiler/CompileStatement.cpp
  src/compiler/InlineGateo.cpp
  src/compiler/NodeEmitter.cpp
  src/compiler/SymbolTable.cpp
  src/compiler/statement/comp_call.cpp
  src/compiler/statement/init.cpp
  src/compiler/statement/mutation.cpp
  src/compiler/statement/return_stmt.cpp
  src/ProgramLoader.cpp
  src/parser/Parser.cpp
)

target_include_directories(gate_lang
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# PUBLIC: headers under include/ may include <peglib.h>; .gateo interop via gateo-cpp
target_link_libraries(gate_lang PUBLIC peglib gateo::gateo)

# --- CLI (compiler driver: parse → CompCache → .gateo under build dir by default) ---
add_executable(gate-lang-cli src/gatec.cpp)
set_target_properties(gate-lang-cli PROPERTIES
  OUTPUT_NAME gatec
)
target_link_libraries(gate-lang-cli PRIVATE gate_lang)
# Default output directory for emitted `.gateo` files (override with argv[2])
target_compile_definitions(gate-lang-cli PRIVATE
  "GATEC_DEFAULT_GATEO_DIR=\"${CMAKE_BINARY_DIR}/gateo-cache\""
)
