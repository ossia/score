project(StateTests)
enable_testing()
find_package(${QT_VERSION} REQUIRED COMPONENTS Core)
find_package(Catch2 QUIET)
function(addStateTest TESTNAME TESTSRCS)
    add_executable(State_${TESTNAME} ${TESTSRCS})
    setup_score_common_test_features(State_${TESTNAME})
    target_link_libraries(State_${TESTNAME} PRIVATE ${QT_PREFIX}::Core score_lib_base score_lib_state  Catch2::Catch2WithMain )
    add_test(State_${TESTNAME}_target State_${TESTNAME})
endFunction()


# Commands
addStateTest(ExpressionTest
             "${CMAKE_CURRENT_SOURCE_DIR}/ExpressionParsingTests.cpp")

addStateTest(SerializationTest
             "${CMAKE_CURRENT_SOURCE_DIR}/SerializationTest.cpp")
addStateTest(EqualityTest
             "${CMAKE_CURRENT_SOURCE_DIR}/EqualityTest.cpp")
addStateTest(UnitTest
             "${CMAKE_CURRENT_SOURCE_DIR}/UnitTests.cpp")

set(CMAKE_AUTOMOC OFF)
