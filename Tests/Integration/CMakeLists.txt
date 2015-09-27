cmake_minimum_required(VERSION 3.0)
project(IntegrationTests)

enable_testing()

function(addIntegrationTest TESTNAME TESTSRCS)
    add_executable(Integration_${TESTNAME} ${TESTSRCS})
    target_link_libraries(Integration_${TESTNAME} iscore_integration_lib)
    add_test(Integration_${TESTNAME}_target Integration_${TESTNAME})
endFunction()

# Commands
addIntegrationTest(Test1
             "${CMAKE_CURRENT_SOURCE_DIR}/Test1.cpp")

