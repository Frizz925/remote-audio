add_executable(ratest pool.c logger.c)
target_link_libraries(ratest lib_logger
                             lib_stream
                             criterion)
add_test(ratest ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ratest)
