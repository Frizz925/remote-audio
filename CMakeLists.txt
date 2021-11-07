cmake_minimum_required(VERSION 3.0)
project(remote-audio C)
enable_testing()

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

set(SOURCES socket.c proto.c)
set(TEST_SOURCES proto.c)

add_executable(remote-audio-sink sink.c ${SOURCES})
target_link_libraries(remote-audio-sink sodium)

add_executable(remote-audio-test test.c ${TEST_SOURCES})
target_link_libraries(remote-audio-test sodium)

add_test(test_proto remote-audio-test proto)
add_test(test_crypto remote-audio-test crypto)
