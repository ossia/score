cmake_minimum_required(VERSION 3.0)
project(StateTests)
set(CMAKE_AUTOMOC ON)
enable_testing()
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Test)

function(addStateTest TESTNAME TESTSRCS)
    add_executable(State_${TESTNAME} ${TESTSRCS})
    setup_score_common_test_features(State_${TESTNAME})
    target_link_libraries(State_${TESTNAME} PRIVATE Qt5::Core Qt5::Test score_lib_base score_lib_state)
    add_test(State_${TESTNAME}_target State_${TESTNAME})
endFunction()


# Commands
addStateTest(ExpressionTest
             "${CMAKE_CURRENT_SOURCE_DIR}/ExpressionParsingTests.cpp")

# addStateTest(SerializationTest
#              "${CMAKE_CURRENT_SOURCE_DIR}/SerializationTest.cpp")
# addStateTest(EqualityTest
#              "${CMAKE_CURRENT_SOURCE_DIR}/EqualityTest.cpp")
# addStateTest(UnitTest
#              "${CMAKE_CURRENT_SOURCE_DIR}/UnitTests.cpp")

set(CMAKE_AUTOMOC OFF)
