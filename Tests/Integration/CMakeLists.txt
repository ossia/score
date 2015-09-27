cmake_minimum_required(VERSION 3.0)
project(IntegrationTests)

enable_testing()
find_package(Qt5 5.5 REQUIRED COMPONENTS Core Gui Widgets Test)

function(addIntegrationTest TESTNAME TESTSRCS)
    add_executable(Integration_${TESTNAME} ${TESTSRCS})
    target_link_libraries(Integration_${TESTNAME} Qt5::Core Qt5::Test iscore_lib_base iscore_plugin_deviceexplorer iscore_plugin_curve iscore_plugin_scenario iscore_plugin_ossia)
    add_test(Integration_${TESTNAME}_target Integration_${TESTNAME})
endFunction()


# Commands
addIntegrationTest(Test1
             "${CMAKE_CURRENT_SOURCE_DIR}/Test1.cpp")

