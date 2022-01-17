cmake_minimum_required(VERSION 3.0)
project(RemoteAudio C)
enable_testing()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if(WIN32)
  include_directories(C:/msys64/mingw64/include)
  link_directories(C:/msys64/mingw64/lib)
else()
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
endif()

include_directories(./src)
include_directories(${CMAKE_SOURCE_DIR}/src)
add_subdirectory(src)
