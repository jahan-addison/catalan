cmake_minimum_required(VERSION 3.16...3.29)

set(CMAKE_POLICY_VERSION_MINIMUM 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(
    FATAL_ERROR
      "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source."
  )
endif()

project(
  cadence
  VERSION 0.0.1
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(GNUInstallDirs)
include(CMakeDependentOption)
include(cmake/CPM.cmake)

include(GNUInstallDirs)

# ##############################################################################
# DEPENDENCIES
# ##############################################################################

cpmaddpackage("gh:StableCoder/cmake-scripts#24.04")
cpmaddpackage("gh:TheLartians/PackageProject.cmake@1.8.0")

cpmaddpackage("gh:doctest/doctest@2.4.12")

include(${cmake-scripts_SOURCE_DIR}/tools.cmake)

# Recommended dependencies:

cpmaddpackage(NAME matchit VERSION 1.0.1 GITHUB_REPOSITORY BowenFu/matchit.cpp)
cpmaddpackage("gh:fmtlib/fmt#12.1.0")

# Uncomment to add my easy-to-use header-only JSON library:

# cpmaddpackage( NAME easyjson VERSION 1.3.1 GITHUB_REPOSITORY
# jahan-addison/easyjson OPTIONS "EASYJSON_BUILD_EXAMPLES OFF"
# "EASYJSON_BUILD_TESTS OFF")

# ##############################################################################
# BUILD OPTIONS
# ##############################################################################

if(MSVC)
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} /Zc:__cplusplus /utf-8 /DEBUG /D_CRT_SECURE_NO_DEPRECATE /W4 /WX"
  )
else()
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -Werror -Wpedantic -DDEBUG")
  else()
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic -O2 -march=native")
  endif()
endif()

# ##############################################################################
# Sanitizers
# ##############################################################################

# Enable sanitizer with `-DUSE_SANITIZER=<VALUE>` argument to CMake Address,
# Memory, MemoryWithOrigins, Undefined, Thread, Leak, 'Address;Undefined'
if(USE_SANITIZER)
  include(${cmake-scripts_SOURCE_DIR}/sanitizers.cmake)
endif()

# ##############################################################################
# Include-what-you-use https://include-what-you-use.org/
# ##############################################################################

if(IWYU)
  find_program(IWYU_EXECUTABLE include-what-you-use)
  if(IWYU_EXECUTABLE)
    if(APPLE)
      set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
          "${IWYU_EXECUTABLE};;-isysroot $(xcrun --sdk macosx --show-sdk-path)")
    else()
      set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_EXECUTABLE};;")
    endif()
    message(STATUS "IWYU enabled for C++ targets.")
  else()
    message(
      WARNING "Include-What-You-Use executable not found. IWYU will not be run."
    )
  endif()
endif()

# ##############################################################################
# clang-tidy (cmake --build build --target tidy)
# ##############################################################################

# Files matching this regex are skipped by the `tidy` target below. Edit it to
# fit your own project layout and generated files.
set(CLANG_TIDY_IGNORE_REGEX
    "^(?!.*/(test|\\.cache)/).*\\.cc$"
    CACHE STRING "run-clang-tidy file filter regex")

find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
find_program(RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy run-clang-tidy.py)
if(CLANG_TIDY_EXECUTABLE AND RUN_CLANG_TIDY_EXECUTABLE)
  include(ProcessorCount)
  ProcessorCount(NPROC)
  set(TIDY_COMPILE_COMMANDS_DIR ${CMAKE_BINARY_DIR}/tidy)
  add_custom_target(
    tidy
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TIDY_COMPILE_COMMANDS_DIR}
    COMMAND
      ${CMAKE_COMMAND}
      -DSOURCE_COMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json
      -DOUTPUT_COMPILE_COMMANDS=${TIDY_COMPILE_COMMANDS_DIR}/compile_commands.json
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dedup_compile_commands.cmake
    COMMAND ${RUN_CLANG_TIDY_EXECUTABLE} -p ${TIDY_COMPILE_COMMANDS_DIR} -j
            ${NPROC} -warnings-as-errors=* "${CLANG_TIDY_IGNORE_REGEX}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-tidy over ${PROJECT_NAME}/ sources"
    VERBATIM)
else()
  message(
    STATUS "clang-tidy or run-clang-tidy not found: `tidy` target unavailable.")
endif()

# ##############################################################################
# Executables
# ##############################################################################

file(GLOB_RECURSE headers CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.cc")
file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cc")

add_executable(${PROJECT_NAME} ${sources} ${headers})

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}>
         $<INSTALL_INTERFACE:${PROJECT_NAME}-${PROJECT_VERSION}>)

target_include_directories(${PROJECT_NAME} PUBLIC fmt::fmt matchit)

target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt matchit)

include(cmake/doctest.cmake)
