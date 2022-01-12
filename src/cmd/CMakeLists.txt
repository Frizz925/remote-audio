add_executable(remote-audio-sink sink.c)
target_link_libraries(remote-audio-sink app-sink)

add_executable(remote-audio-source source.c)
target_link_libraries(remote-audio-source app-source)

if(WIN32)
  add_executable(remote-audio-svc win32/service.c)
  target_link_libraries(remote-audio-svc app-sink app-source)
endif()
