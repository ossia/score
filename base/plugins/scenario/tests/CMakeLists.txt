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
addIScoreQtTest(AddProcessToConstraintTest
				"${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddProcessToConstraintTest.cpp")
addIScoreQtTest(AddBoxToConstraintTest
				"${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddBoxToConstraintTest.cpp")
addIScoreQtTest(AddDeckToBoxTest
				"${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddDeckToBoxTest.cpp")
addIScoreQtTest(AddProcessViewModelToDeckTest
				"${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddProcessViewModelToDeckTest.cpp")

addIScoreQtTest(CreateEventAfterEventTest
				"${CMAKE_CURRENT_SOURCE_DIR}/Commands/CreateEventAfterEventTest.cpp")

addIScoreQtTest(MoveEventCommandTest
				"${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveEventTest.cpp")
addIScoreQtTest(MoveConstraintCommandTest
				"${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveConstraintTest.cpp")

## Constraints
addIScoreQtTest(ConstraintModelTest
				"${CMAKE_CURRENT_SOURCE_DIR}/Constraint/ConstraintModelTests.cpp")

include(CTest)
