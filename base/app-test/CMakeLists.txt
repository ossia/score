project(score_testapp LANGUAGES CXX)

enable_testing()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestObject.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestApplication.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestApplication.cpp"
    )



add_executable(score_testapp ${SRCS})

target_link_libraries(score_testapp PRIVATE score_lib_base ${SCORE_PLUGINS_LIST}
    Qt5::Core Qt5::Widgets Qt5::Gui Qt5::Network Qt5::Xml Qt5::Svg Qt5::Test)

if(SCORE_STATIC_PLUGINS)
  target_link_libraries(score_testapp PUBLIC ${SCORE_PLUGINS_LIST})
endif()

setup_score_common_exe_features(score_testapp)

setup_target_for_coverage(
    score_test_coverage
    score_testapp
    coverage_out)


if(TARGET score_testapp_unity)
    setup_target_for_coverage(
        score_test_coverage_unity
        score_testapp_unity
        coverage)
endif()
