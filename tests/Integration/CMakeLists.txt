project(IntegrationTests)

find_package(Qt5 5.9 COMPONENTS Test)
enable_testing()
include(CTest)
set(CMAKE_AUTOMOC ON)
function(add_integration_test TESTNAME TESTSRCS)
    add_executable(Integration_${TESTNAME} ${TESTSRCS} score_integration.hpp)
    target_link_libraries(Integration_${TESTNAME} PRIVATE ${SCORE_PLUGINS_LIST} Qt5::Test)
    target_include_directories(Integration_${TESTNAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(Integration_${TESTNAME}_target Integration_${TESTNAME})
endFunction()

add_integration_test(SerializationTest "${CMAKE_CURRENT_SOURCE_DIR}/SerializationTest.cpp")
add_integration_test(PortSerializationTest "${CMAKE_CURRENT_SOURCE_DIR}/PortSerializationTest.cpp")
# Commands

# addIntegrationTest(Test1
#              "${CMAKE_CURRENT_SOURCE_DIR}/Test1.cpp")
# addIntegrationTest(TestStatesMinMax
#              "${CMAKE_CURRENT_SOURCE_DIR}/States/TestStatesMinMax.cpp")
# addIntegrationTest(TestStatesCurve
#              "${CMAKE_CURRENT_SOURCE_DIR}/States/TestStatesCurve.cpp")
# addIntegrationTest(TestStatesDragDrop
# 			 "${CMAKE_CURRENT_SOURCE_DIR}/States/TestStatesDragDrop.cpp")


function(add_score_test _name _file)
  add_executable(${_name} ${_file})

  target_link_libraries(${_name} PRIVATE score_lib_base ${SCORE_PLUGINS_LIST}
      Qt5::Core Qt5::Widgets Qt5::Gui Qt5::Network Qt5::Xml Qt5::Test)

  if(TARGET Qt5::Svg)
    target_link_libraries(score_lib_base
      PRIVATE Qt5::Svg
    )
  endif()

  target_include_directories(${_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  setup_score_common_exe_features(${_name})

  if(SCORE_COVERAGE)
    setup_target_for_coverage(
        score_test_coverage
        score_testapp
        "/usr;/opt;libossia/3rdparty;moc_;ui_;.moc;qrc_"
        coverage_out)

    if(TARGET score_testapp_unity)
        setup_target_for_coverage(
            score_test_coverage_unity
            score_testapp_unity
            "/usr;/opt;libossia/3rdparty;moc_;ui_;.moc;qrc_"
            coverage)
    endif()
  endif()
endfunction()

add_score_test(score_test_stacks "${CMAKE_CURRENT_SOURCE_DIR}/test-stacks.cpp")
add_score_test(score_test_selection "${CMAKE_CURRENT_SOURCE_DIR}/test-selection.cpp")
add_score_test(score_test_removal "${CMAKE_CURRENT_SOURCE_DIR}/test-removal.cpp")
add_score_test(score_test_processes "${CMAKE_CURRENT_SOURCE_DIR}/test-all-processes.cpp")
add_score_test(score_test_processes_exec "${CMAKE_CURRENT_SOURCE_DIR}/test-all-processes-exec.cpp")
