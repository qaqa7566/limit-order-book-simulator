cmake_minimum_required(VERSION 3.16)

project(limit_order_book_simulator
        VERSION 0.1.0
        DESCRIPTION "Price-time priority limit order book simulator"
        LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Language standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimized build for benchmarking convenience.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  message(STATUS "No build type specified; defaulting to Release")
endif()

# ---------------------------------------------------------------------------
# Warnings (attached to our own targets, not third-party dependencies)
# ---------------------------------------------------------------------------
add_library(lob_warnings INTERFACE)
if(MSVC)
  target_compile_options(lob_warnings INTERFACE /W4 /permissive-)
else()
  target_compile_options(lob_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wdouble-promotion)
endif()

# ---------------------------------------------------------------------------
# Core library: order book + matching engine
# ---------------------------------------------------------------------------
add_library(lob_core
  src/order_book.cpp
  src/matching_engine.cpp)
target_include_directories(lob_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lob_core PRIVATE lob_warnings)

# ---------------------------------------------------------------------------
# CLI executable
# ---------------------------------------------------------------------------
add_executable(lob_sim src/main.cpp)
target_link_libraries(lob_sim PRIVATE lob_core lob_warnings)

# ---------------------------------------------------------------------------
# Benchmark executable
# ---------------------------------------------------------------------------
add_executable(benchmark_matching benchmarks/benchmark_matching.cpp)
target_link_libraries(benchmark_matching PRIVATE lob_core lob_warnings)

# ---------------------------------------------------------------------------
# Tests (GoogleTest). Uses a system install if present, otherwise fetches a
# pinned release. Toggle with -DLOB_BUILD_TESTS=OFF.
# ---------------------------------------------------------------------------
option(LOB_BUILD_TESTS "Build unit tests" ON)
if(LOB_BUILD_TESTS)
  enable_testing()

  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    # Allow the pinned third-party project to configure under CMake >= 4.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.15.2
      GIT_SHALLOW    TRUE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(lob_tests
    tests/test_order_book.cpp
    tests/test_matching_engine.cpp)
  target_link_libraries(lob_tests PRIVATE lob_core GTest::gtest_main lob_warnings)

  include(GoogleTest)
  gtest_discover_tests(lob_tests)
endif()
