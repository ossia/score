cmake_minimum_required(VERSION 3.0)
project(ScenarioTests)

include_directories("${CMAKE_SOURCE_DIR}/base")

find_package(Qt5Core)
find_package(Qt5Test)

function(addIScoreQtTest TESTNAME TESTSRCS)
	add_executable(${TESTNAME} ${TESTSRCS})
	target_link_libraries(${TESTNAME} Qt5::Core Qt5::Test scenario_process)
	add_test(${TESTNAME}_target ${TESTNAME})
endFunction()

# Commands
addIScoreQtTest(addprocesstointerval_command_test "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddProcessToIntervalCommandTest.cpp")
#addIScoreQtTest(createeventafterevent_command_test "${CMAKE_CURRENT_SOURCE_DIR}/CreateEventAfterEventCommandTest.cpp")
#addIScoreQtTest(moveevent_command_test "${CMAKE_CURRENT_SOURCE_DIR}/MoveEventCommandTest.cpp")
#addIScoreQtTest(moveinterval_command_test "${CMAKE_CURRENT_SOURCE_DIR}/MoveIntervalCommandTest.cpp")

# Intervals
addIScoreQtTest(interval_test "${CMAKE_CURRENT_SOURCE_DIR}/Interval/IntervalModelTests.cpp")

include(CTest)
