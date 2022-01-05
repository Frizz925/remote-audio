include_directories(../src)
set(LIB_SOURCE_DIR ../src/lib)

if(WIN32)
  set(TEST_LIBRARIES ws2_32)
endif()

macro(define_test TEST_NAME TEST_SOURCE)
  add_executable(${TEST_NAME} ${TEST_SOURCE} ${ARGN})
  target_link_libraries(${TEST_NAME} ${TEST_LIBRARIES})
  add_test(${TEST_NAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TEST_NAME})
endmacro()

define_test(ratest-logger ratest_logger.c ${LIB_SOURCE_DIR}/logger.c)
define_test(ratest-utils ratest_utils.c ${LIB_SOURCE_DIR}/utils.c)

if(WIN32)
  define_test(ratest-types ratest_types.c ${LIB_SOURCE_DIR}/types.c ${LIB_SOURCE_DIR}/win32/types.c)
else()
  define_test(ratest-types ratest_types.c ${LIB_SOURCE_DIR}/types.c)
endif()
