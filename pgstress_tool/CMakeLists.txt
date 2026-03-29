cmake_minimum_required(VERSION 3.18)
project(pgstress_tool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_WARNINGS "Enable extra compiler warnings" ON)

if(ENABLE_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
  endif()
endif()

find_package(Threads REQUIRED)

set(LIBPQ_ROOT "" CACHE PATH "Root directory of libpq installation")
set(_libpq_hints)

if(LIBPQ_ROOT)
  list(APPEND _libpq_hints "${LIBPQ_ROOT}")
endif()

if(DEFINED ENV{LIBPQ_ROOT} AND NOT "$ENV{LIBPQ_ROOT}" STREQUAL "")
  list(APPEND _libpq_hints "$ENV{LIBPQ_ROOT}")
endif()

if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
  list(APPEND _libpq_hints "$ENV{HOME}/Desktop/Codes/.local/pg")
endif()

find_path(LIBPQ_INCLUDE_DIR
  NAMES libpq-fe.h
  HINTS ${_libpq_hints}
  PATH_SUFFIXES include)

find_library(LIBPQ_LIBRARY
  NAMES pq libpq
  HINTS ${_libpq_hints}
  PATH_SUFFIXES lib)

if(NOT LIBPQ_INCLUDE_DIR OR NOT LIBPQ_LIBRARY)
  message(FATAL_ERROR "libpq not found. Set -DLIBPQ_ROOT=/path/to/libpq (contains include/lib)")
endif()

add_executable(
  pgstress
  src/main.cpp
  src/config.cpp
  src/db.cpp
  src/workload.cpp
  src/runner.cpp
)
target_include_directories(pgstress PRIVATE "${LIBPQ_INCLUDE_DIR}")
target_link_libraries(pgstress PRIVATE "${LIBPQ_LIBRARY}" Threads::Threads)

if(APPLE)
  get_filename_component(LIBPQ_LIB_DIR "${LIBPQ_LIBRARY}" DIRECTORY)
  target_link_options(pgstress PRIVATE "-Wl,-rpath,${LIBPQ_LIB_DIR}")
endif()
