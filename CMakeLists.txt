cmake_minimum_required(VERSION 3.0)
project(RemoteAudio C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(WIN32)
  include_directories(C:/msys64/mingw64/include)
  link_directories(C:/msys64/mingw64/lib)
else()
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
endif()

add_subdirectory(src)
add_subdirectory(test)
file(INSTALL config.ini DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
