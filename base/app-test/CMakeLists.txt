cmake_minimum_required(VERSION 3.0)
project(iscore_testapp LANGUAGES CXX)

enable_testing()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestObject.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestApplication.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestApplication.cpp"
    )



add_executable(iscore_testapp ${SRCS})

target_link_libraries(iscore_testapp PRIVATE iscore_lib_base   ${ISCORE_PLUGINS_LIST}
    Qt5::Core Qt5::Widgets Qt5::Gui Qt5::Network Qt5::Xml Qt5::Svg Qt5::Test)

if(ISCORE_STATIC_PLUGINS)
  iscore_write_static_plugins_header()
  target_link_libraries(iscore_testapp PUBLIC ${ISCORE_PLUGINS_LIST})
endif()

setup_iscore_common_exe_features(iscore_testapp)

setup_target_for_coverage(
    iscore_test_coverage
    iscore_testapp
    coverage_out)


if(TARGET iscore_testapp_unity)
    setup_target_for_coverage(
        iscore_test_coverage_unity
        iscore_testapp_unity
        coverage)
endif()
