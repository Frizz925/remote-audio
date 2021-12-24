cmake_minimum_required(VERSION 3.0)
project(RemoteAudio C)

set(SOURCES src/types.c src/utils.c src/crypto.c src/stream.c src/audio.c)
set(LINK_LIBRARIES uv sodium portaudio opus)

if(UNIX)
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
endif()

add_executable(remote-audio-sink src/sink.c ${SOURCES})
target_link_libraries(remote-audio-sink ${LINK_LIBRARIES})

add_executable(remote-audio-source src/source.c ${SOURCES})
target_link_libraries(remote-audio-source ${LINK_LIBRARIES})
