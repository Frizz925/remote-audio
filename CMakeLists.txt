cmake_minimum_required(VERSION 3.0)
project(RemoteAudio C)

enable_testing()

set(SOURCES src/common.c src/socket.c src/audio.c src/ring_buffer.c)
set(TEST_SOURCES src/ring_buffer.c)
set(LINK_LIBRARIES portaudio opus)

if(WIN32)
    set(LINK_LIBRARIES ${LINK_LIBRARIES} ws2_32)
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

add_executable(remote-audio-test test/test_runner.c ${TEST_SOURCES})
add_test(RingBufferTest remote-audio-test ring_buffer_test)
