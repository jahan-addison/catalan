# ##############################################################################
# Copyright (c) 2026 Jahan Addison
#
# License: Apache 2.0
#
# See the LICENSE file in the project root for the full text.
# ##############################################################################

cmake_minimum_required(VERSION 3.16...3.31)

set(CMAKE_POLICY_VERSION_MINIMUM 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(
    FATAL_ERROR
      "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source."
  )
endif()

project(
  catalan
  VERSION 1.1.1
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Build type" FORCE)
endif()

option(CATALAN_BUILD_TESTS "Build the doctest suite" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(cmake/CPM.cmake)

# ##############################################################################
# Dependencies
# ##############################################################################

cpmaddpackage("gh:StableCoder/cmake-scripts#24.04.1")

include(${cmake-scripts_SOURCE_DIR}/tools.cmake)

# sqlite_orm::sqlite_orm links SQLite3 transitively.
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

# libcurl verifies Turnstile tokens with Cloudflare, see catalan/turnstile.hh.
find_package(CURL REQUIRED)

# asio has no CMakeLists, so asio::asio is defined here before Crow's
# Findasio.cmake creates a directory-local target this project can't see.
cpmaddpackage(
  NAME
  asio
  GITHUB_REPOSITORY
  chriskohlhoff/asio
  GIT_TAG
  asio-1-38-2
  DOWNLOAD_ONLY
  YES)

set(ASIO_INCLUDE_DIR
    "${asio_SOURCE_DIR}/asio/include"
    CACHE PATH "Asio include dir" FORCE)

add_library(asio INTERFACE)
add_library(asio::asio ALIAS asio)
target_include_directories(asio SYSTEM INTERFACE ${ASIO_INCLUDE_DIR})
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
target_link_libraries(asio INTERFACE Threads::Threads)

cpmaddpackage(
  NAME
  Crow
  GITHUB_REPOSITORY
  CrowCpp/Crow
  GIT_TAG
  v1.3.4
  OPTIONS
  "CROW_BUILD_EXAMPLES OFF"
  "CROW_BUILD_TESTS OFF"
  "CROW_INSTALL OFF")

cpmaddpackage("gh:fnc12/sqlite_orm#v1.9.1")

# ##############################################################################
# Examples
# ##############################################################################

# examples/*.md are the seed posts and the editor's example. The server gets
# them through a generated header, so there is one copy of each.
set(CATALAN_EXAMPLES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/examples")
file(READ "${CATALAN_EXAMPLES_DIR}/euler-identity.md"
     CATALAN_EXAMPLE_EULER_IDENTITY)
file(READ "${CATALAN_EXAMPLES_DIR}/basel-problem.md"
     CATALAN_EXAMPLE_BASEL_PROBLEM)
set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS "${CATALAN_EXAMPLES_DIR}/euler-identity.md"
           "${CATALAN_EXAMPLES_DIR}/basel-problem.md")

set(CATALAN_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(cmake/examples.hh.in
               "${CATALAN_GENERATED_DIR}/catalan/examples.hh" @ONLY)

# ##############################################################################
# Warnings
# ##############################################################################

# Applied per-target rather than via CMAKE_CXX_FLAGS, so -Werror never fires on
# dependencies. No -march=native, since release binaries run on the deploy
# target's CPU, not the build machine's.
set(CATALAN_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# ##############################################################################
# Sanitizers
# ##############################################################################

# Enable sanitizer with `-DUSE_SANITIZER=<VALUE>` argument to CMake Address,
# Memory, MemoryWithOrigins, Undefined, Thread, Leak, 'Address;Undefined'
if(USE_SANITIZER)
  include(${cmake-scripts_SOURCE_DIR}/sanitizers.cmake)
endif()

# ##############################################################################
# Executables
# ##############################################################################

file(GLOB_RECURSE headers CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.hh")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/*.cc")
file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cc")

add_executable(${PROJECT_NAME} ${sources} ${headers})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                   ${CATALAN_GENERATED_DIR})

target_compile_options(${PROJECT_NAME} PRIVATE ${CATALAN_WARNINGS})
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

target_link_libraries(${PROJECT_NAME} PRIVATE sqlite_orm::sqlite_orm Crow::Crow
                                              CURL::libcurl)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

if(CATALAN_BUILD_TESTS)
  cpmaddpackage("gh:doctest/doctest@2.4.12")
  include(cmake/doctest.cmake)
endif()
