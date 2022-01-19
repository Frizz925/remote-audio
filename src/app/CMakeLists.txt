add_library(app_sink sink.c)
target_link_libraries(app_sink lib_logger
                               lib_socket
                               portaudio
                               uv)
