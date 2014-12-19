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
addIScoreQtTest(addprocesstoconstraint_command_test "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddProcessToConstraintCommandTest.cpp")
#addIScoreQtTest(createeventafterevent_command_test "${CMAKE_CURRENT_SOURCE_DIR}/Commands/CreateEventAfterEventCommandTest.cpp")
#addIScoreQtTest(moveevent_command_test "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveEventCommandTest.cpp")
#addIScoreQtTest(moveconstraint_command_test "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveConstraintCommandTest.cpp")

## Constraints
#addIScoreQtTest(constraint_test "${CMAKE_CURRENT_SOURCE_DIR}/Constraint/ConstraintModelTests.cpp")

include(CTest)
