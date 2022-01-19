add_library(lib_pool pool.c)

add_library(lib_stream stream.c)
target_link_libraries(lib_stream uv)

add_library(lib_logger logger.c)
target_link_libraries(lib_logger lib_pool uv)

add_library(lib_socket socket.c)
if(WIN32)
  target_link_libraries(lib_socket uv ws2_32)
else()
  target_link_libraries(lib_socket uv)
endif()
