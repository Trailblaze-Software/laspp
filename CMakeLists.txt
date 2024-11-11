cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(
  LAS++
  VERSION 0.0.0
  LANGUAGES CXX)
set(COPYRIGHT "Copyright (c) 2024 Trailblaze Software. All rights reserved.")

add_compile_definitions(LASPP_VERSION="${PROJECT_VERSION}")
execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE)
add_compile_definitions(GIT_COMMIT_HASH="${GIT_COMMIT_HASH}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(WIN32)
  if(${CMAKE_CXX_COMPILER} MATCHES ".*mingw.*")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fstrict-enums -Wall -Wextra -Wpedantic -Werror")
  else()
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} /openmp:llvm /Wall /MP /wd5045 /wd4866 /wd4514 /wd4820 /wd4800 /wd4242 /wd4244 /wd4365 /wd5027 /wd4626 /wd5219 /wd4623 /wd4267 /wd4710 /wd4711 /wd4625 /wd5026"
    )
  endif()
elseif(UNIX)
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fstrict-enums -Wall -Wextra -Wpedantic -Werror")
else()
  message(FATAL_ERROR "Unsupported platform")
endif()

set(LIBRARY_NAME las++)
set(EXE_NAME las++-cli)

include_directories(src)

add_library(${LIBRARY_NAME})
add_executable(${EXE_NAME})

find_package(OpenMP REQUIRED)

target_link_libraries(${EXE_NAME} PRIVATE ${LIBRARY_NAME} OpenMP::OpenMP_CXX)

add_subdirectory(src)

include(GNUInstallDirs)

install(TARGETS ${EXE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  TARGETS ${LIBRARY_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
