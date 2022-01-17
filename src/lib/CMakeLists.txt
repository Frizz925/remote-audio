add_library(lib_pool pool.c)
target_link_libraries(lib_pool uv)

add_library(lib_logger logger.c)
target_link_libraries(lib_logger lib_pool)
