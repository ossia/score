cmake_minimum_required(VERSION 3.0)
project(IntegrationTests)

enable_testing()
include(CTest)

add_library(score_integration_lib STATIC IscoreIntegrationTests.cpp)
target_link_libraries(score_integration_lib score_lib_base)
function(add_integration_test TESTNAME TESTSRCS)
    add_executable(Integration_${TESTNAME} ${TESTSRCS} IscoreIntegrationTests.hpp)
    target_link_libraries(Integration_${TESTNAME} PRIVATE score_lib_base Qt5::Core Qt5::Widgets Qt5::Test score_integration_lib)
    target_include_directories(Integration_${TESTNAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(Integration_${TESTNAME}_target Integration_${TESTNAME})
endFunction()


file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/TestData" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

add_integration_test(SerializationTest "${CMAKE_CURRENT_SOURCE_DIR}/SerializationTest.cpp")
# Commands

# addIntegrationTest(Test1
#              "${CMAKE_CURRENT_SOURCE_DIR}/Test1.cpp")
# addIntegrationTest(TestStatesMinMax
#              "${CMAKE_CURRENT_SOURCE_DIR}/States/TestStatesMinMax.cpp")
# addIntegrationTest(TestStatesCurve
#              "${CMAKE_CURRENT_SOURCE_DIR}/States/TestStatesCurve.cpp")
# addIntegrationTest(TestStatesDragDrop
# 			 "${CMAKE_CURRENT_SOURCE_DIR}/States/TestStatesDragDrop.cpp")

