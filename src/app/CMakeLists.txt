set(libraries lib sodium portaudio opus)

if(WIN32)
  list(APPEND libraries ws2_32)
endif()

add_library(app-sink STATIC sink.c)
target_link_libraries(app-sink ${libraries})

add_library(app-source STATIC source.c)
target_link_libraries(app-source ${libraries})
