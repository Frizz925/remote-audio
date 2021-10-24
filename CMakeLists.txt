cmake_minimum_required(VERSION 3.0)
project(RemoteAudio C)

set(SOURCES src/common.c src/socket.c src/audio.c)
set(LINK_LIBRARIES portaudio opus)

if(WIN32)
    set(LINK_LIBRARIES ${LINK_LIBRARIES} ws2_32)
    include_directories(C:/msys64/mingw64/lib)
    link_directories(C:/msys64/mingw64/lib)
endif()

add_executable(remote-audio-sink src/sink.c ${SOURCES})
target_link_libraries(remote-audio-sink ${LINK_LIBRARIES})

add_executable(remote-audio-source src/source.c ${SOURCES})
target_link_libraries(remote-audio-source ${LINK_LIBRARIES})