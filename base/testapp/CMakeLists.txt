cmake_minimum_required(VERSION 3.0)
project(iscore_testapp)

enable_testing()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib/core")

set(SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestObject.hpp"
    )

add_executable(iscore_testapp ${SRCS})

target_link_libraries(iscore_testapp iscore_lib_base)

setup_iscore_common_features(iscore_testapp)

if(ISCORE_COVERAGE)
  setup_target_for_coverage(
      iscore_test_coverage
      iscore_testapp
      coverage_out)
endif()
