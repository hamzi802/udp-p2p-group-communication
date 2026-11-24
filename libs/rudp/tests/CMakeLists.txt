# cmake_minimum_required(VERSION 3.16)

# project(UDPTests LANGUAGES CXX)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

# add_executable(server
#     ../src/server.cpp
#     ../src/protocol.cpp
# )

# add_executable(client
#     ../src/client.cpp
#     ../src/protocol.cpp
# )

# target_include_directories(server PRIVATE ../include)
# target_include_directories(client PRIVATE ../include)


# Put this file in  libs/rudp/tests/CMakeLists.txt   (next to the build/ folder).
#
#   cd libs/rudp/tests/build
#   cmake ..
#   cmake --build .
#
# It compiles EVERY .cpp found anywhere under  libs/rudp/src  into a library, and
# builds the two programs (server, client) from the files named server.cpp / client.cpp.
# Header folders under include/ and src/ are found automatically.
cmake_minimum_required(VERSION 3.16)

project(UDPTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra)

# ---- where things are (paths are relative to THIS file's folder, tests/) ----------------
get_filename_component(RUDP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(RUDP_INCLUDE_DIR "${RUDP_DIR}/include")
set(RUDP_SRC_DIR     "${RUDP_DIR}/src")

# ---- include directories ----------------------------------------------------------------
# include/ itself (so  #include "protocol/packet.h"  works), plus every folder that holds a
# header, so  #include "client.h"  works wherever client.h lives.
file(GLOB_RECURSE ALL_HEADERS CONFIGURE_DEPENDS
     "${RUDP_INCLUDE_DIR}/*.h" "${RUDP_INCLUDE_DIR}/*.hpp"
     "${RUDP_SRC_DIR}/*.h"     "${RUDP_SRC_DIR}/*.hpp")

set(INCLUDE_DIRS "${RUDP_INCLUDE_DIR}" "${RUDP_SRC_DIR}")
foreach(header IN LISTS ALL_HEADERS)
  get_filename_component(header_dir "${header}" DIRECTORY)
  list(APPEND INCLUDE_DIRS "${header_dir}")
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

# ---- sources ----------------------------------------------------------------------------
# Every .cpp under src/ (any depth).
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS "${RUDP_SRC_DIR}/*.cpp")

# The two programs each have their own main(), so they cannot go in the shared library.
set(SERVER_MAIN "")
set(CLIENT_MAIN "")
foreach(source IN LISTS ALL_SOURCES)
  get_filename_component(source_name "${source}" NAME)
  if(source_name STREQUAL "server.cpp")
    set(SERVER_MAIN "${source}")
  elseif(source_name STREQUAL "client.cpp")
    set(CLIENT_MAIN "${source}")
  endif()
endforeach()

if(NOT SERVER_MAIN OR NOT CLIENT_MAIN)
  message(FATAL_ERROR "Could not find server.cpp and client.cpp under ${RUDP_SRC_DIR}")
endif()

set(LIBRARY_SOURCES ${ALL_SOURCES})
list(REMOVE_ITEM LIBRARY_SOURCES "${SERVER_MAIN}" "${CLIENT_MAIN}")

if(NOT LIBRARY_SOURCES)
  message(FATAL_ERROR "No library sources (packet.cpp, crc32.cpp, ...) found under ${RUDP_SRC_DIR}")
endif()

message(STATUS "Library sources:")
foreach(source IN LISTS LIBRARY_SOURCES)
  message(STATUS "  ${source}")
endforeach()
message(STATUS "Server main: ${SERVER_MAIN}")
message(STATUS "Client main: ${CLIENT_MAIN}")

# ---- targets ----------------------------------------------------------------------------
add_library(rudp STATIC ${LIBRARY_SOURCES})
target_include_directories(rudp PUBLIC ${INCLUDE_DIRS})

add_executable(server ${SERVER_MAIN})
target_link_libraries(server PRIVATE rudp)

add_executable(client ${CLIENT_MAIN})
target_link_libraries(client PRIVATE rudp)