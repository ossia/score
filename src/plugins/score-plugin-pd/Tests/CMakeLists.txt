project(PdTests)

set(CMAKE_AUTOMOC ON)
enable_testing()

function(addPdTest TESTNAME TESTSRCS)
    add_executable(Pd_${TESTNAME} ${TESTSRCS})
    setup_score_common_test_features(Pd_${TESTNAME})
    target_link_libraries(Pd_${TESTNAME} PRIVATE ${QT_PREFIX}::Core score_lib_base score_addon_pd)
    add_test(Pd_${TESTNAME}_target Pd_${TESTNAME})
endFunction()


# Commands
addPdTest(DataflowTest
             "${CMAKE_CURRENT_SOURCE_DIR}/PdDataflowTest.cpp")
