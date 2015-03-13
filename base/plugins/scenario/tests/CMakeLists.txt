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

add_custom_target(ScenarioTestHeaders
                                                                        SOURCES
                                                                                "${CMAKE_CURRENT_SOURCE_DIR}/helpers/FriendDeclaration.hpp"
                                                                                "${CMAKE_CURRENT_SOURCE_DIR}/helpers/ForwardDeclaration.hpp"
)

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
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventAfterEventTest.cpp")
addIScoreQtTest(CreateEventTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventTest.cpp")
addIScoreQtTest(CreateEventAfterEventOnTimeNodeTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventAfterEventOnTimeNodeTest.cpp")

addIscoreQtTest(RemoveEventTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementDeletion/RemoveEventTest.cpp")

addIScoreQtTest(HideBoxInViewModelTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/HideBoxInViewModelTest.cpp")
addIScoreQtTest(ShowBoxInViewModelTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ShowBoxInViewModelTest.cpp")

addIScoreQtTest(MoveEventTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveEventTest.cpp")
addIScoreQtTest(MoveConstraintTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveConstraintTest.cpp")

addIScoreQtTest(RemoveBoxFromConstraintTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveBoxFromConstraintTest.cpp")
addIScoreQtTest(RemoveDeckFromBoxTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveDeckFromBoxTest.cpp")
addIScoreQtTest(RemoveProcessFromConstraintTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveProcessFromConstraintTest.cpp")
addIScoreQtTest(RemoveProcessViewFromDeckTest # TODO Rename in RemoveProcessViewModel
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveProcessViewFromDeckTest.cpp")
addIScoreQtTest(ResizeDeckVerticallyTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ResizeDeckVerticallyTest.cpp")

## Constraints
addIScoreQtTest(ConstraintModelTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/Constraint/ConstraintModelTests.cpp")

## TimeNodes
addIScoreQtTest(TimeNodeModelTest
                                "${CMAKE_CURRENT_SOURCE_DIR}/TimeNode/TimeNodeModelTests.cpp")

include(CTest)
