include_directories(../src)
enable_testing()

if(WIN32)
  set(TEST_LIBRARIES ws2_32)
endif()

macro(define_test TEST_NAME TEST_SOURCE)
  add_executable(${TEST_NAME} ${TEST_SOURCE} ${ARGN})
  target_link_libraries(${TEST_NAME} ${TEST_LIBRARIES})
  add_test(${TEST_NAME} ${TEST_NAME})
endmacro()

define_test(ratest-types ratest_types.c ../src/lib/types.c)
define_test(ratest-utils ratest_utils.c ../src/lib/utils.c)
