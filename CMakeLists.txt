cmake_minimum_required(VERSION 3.0)

project(RemoteAudio CXX)

set(SOURCES src/message.c
            src/crypto.c
            src/utils.c)
set(LINK_LIBRARIES sodium)

if(UNIX)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif()

add_executable(remote-audio-sink src/sink.c ${SOURCES})
target_link_libraries(remote-audio-sink ${LINK_LIBRARIES})

add_executable(remote-audio-source src/source.c ${SOURCES})
target_link_libraries(remote-audio-source ${LINK_LIBRARIES})
