cmake_minimum_required(VERSION 3.0)
project(ProcessTests)

enable_testing()
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Test)

function(addProcessTest TESTNAME TESTSRCS)
    add_executable(Process_${TESTNAME} ${TESTSRCS})
    setup_score_common_test_features(Process_${TESTNAME})
    target_link_libraries(Process_${TESTNAME} PRIVATE Qt5::Core Qt5::Test score_lib_base score_lib_process)
    add_test(Process_${TESTNAME}_target Process_${TESTNAME})
endFunction()


# Commands
addProcessTest(PortSerializationTest
             "${CMAKE_CURRENT_SOURCE_DIR}/PortSerializationTest.cpp")
