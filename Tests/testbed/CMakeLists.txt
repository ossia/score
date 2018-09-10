project(score_testbed LANGUAGES CXX)

score_common_setup()
set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/testbed.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/testbed.cpp"
)

add_executable(testbed ${SRCS})
target_link_libraries(testbed PUBLIC score_lib_base  ${SCORE_PLUGINS_LIST})
setup_score_common_exe_features(testbed)
