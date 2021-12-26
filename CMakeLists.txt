cmake_minimum_required(VERSION 3.0)
project(RemoteAudio C)
enable_testing()

set(SOURCES src/types.c src/utils.c src/crypto.c src/socket.c src/stream.c src/audio.c)
set(LINK_LIBRARIES sodium portaudio opus)

if(WIN32)
  set(LINK_LIBRARIES ${LINK_LIBRARIES} ws2_32)
  set(TEST_LIBRARIES ws2_32)

  include_directories(C:/msys64/mingw64/include)
  link_directories(C:/msys64/mingw64/lib)
else()
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
endif()

add_executable(remote-audio-sink src/sink.c ${SOURCES})
target_link_libraries(remote-audio-sink ${LINK_LIBRARIES})

add_executable(remote-audio-source src/source.c ${SOURCES})
target_link_libraries(remote-audio-source ${LINK_LIBRARIES})

macro(define_test TEST_NAME TEST_SOURCE)
  add_executable(${TEST_NAME} ${TEST_SOURCE} ${ARGN})
  target_link_libraries(${TEST_NAME} ${TEST_LIBRARIES})
  add_test(${TEST_NAME} ${TEST_NAME})
endmacro()

define_test(ratest-types test/ratest_types.c src/types.c)
define_test(ratest-utils test/ratest_utils.c src/utils.c)
