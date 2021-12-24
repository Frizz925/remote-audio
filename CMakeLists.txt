cmake_minimum_required(VERSION 3.0)
project(RemoteAudio C)

set(SOURCES src/common.c src/crypto.c src/types.c)
set(LINK_LIBRARIES uv sodium portaudio)

if(UNIX)
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
endif()

add_executable(remote-audio-sink src/sink.c ${SOURCES})
target_link_libraries(remote-audio-sink ${LINK_LIBRARIES})

add_executable(remote-audio-source src/source.c ${SOURCES})
target_link_libraries(remote-audio-source ${LINK_LIBRARIES})
