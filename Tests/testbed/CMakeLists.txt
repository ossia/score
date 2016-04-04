cmake_minimum_required(VERSION 3.0)
project(iscore_testbed LANGUAGES CXX)

iscore_common_setup()
set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/testbed.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/testbed.cpp"
)

add_executable(testbed ${SRCS})
target_link_libraries(testbed PUBLIC iscore_lib_base  ${ISCORE_PLUGINS_LIST})
setup_iscore_common_exe_features(testbed)
