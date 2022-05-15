project(StateTests)
enable_testing()
find_package(${QT_VERSION} REQUIRED COMPONENTS Core)

function(addDeviceTest TESTNAME TESTSRCS)
    add_executable(Device_${TESTNAME} ${TESTSRCS})
    setup_score_common_test_features(Device_${TESTNAME})
    target_link_libraries(Device_${TESTNAME} PRIVATE ${QT_PREFIX}::Core score_lib_base score_lib_state score_lib_device)
    add_test(Device_${TESTNAME}_target Device_${TESTNAME})
endFunction()


addDeviceTest(SerializationTest
             "${CMAKE_CURRENT_SOURCE_DIR}/SerializationTest.cpp")

set(CMAKE_AUTOMOC OFF)
