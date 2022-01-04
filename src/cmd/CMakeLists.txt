set(libraries lib sodium portaudio opus)

if(WIN32)
  list(APPEND libraries ws2_32)
endif()

add_executable(remote-audio-sink sink.c)
target_link_libraries(remote-audio-sink ${libraries})

add_executable(remote-audio-source source.c)
target_link_libraries(remote-audio-source ${libraries})

if(WIN32)
  add_executable(remote-audio-daemon daemon.c)
endif()
