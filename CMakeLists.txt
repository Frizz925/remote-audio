cmake_minimum_required(VERSION 3.0)
project(RemoteAudio C)

set(SOURCES src/socket.c src/audio.c)
set(LINK_LIBRARIES soundio)
if(WIN32)
    set(LINK_LIBRARIES ${LINK_LIBRARIES} ws2_32)
    include_directories(C:/msys64/mingw64/include)
    link_directories(C:/msys64/mingw64/lib)
else()
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif()

add_executable(remote-audio-sink src/sink.c ${SOURCES})
add_executable(remote-audio-source src/source.c ${SOURCES})

target_link_libraries(remote-audio-sink ${LINK_LIBRARIES})
target_link_libraries(remote-audio-source ${LINK_LIBRARIES})