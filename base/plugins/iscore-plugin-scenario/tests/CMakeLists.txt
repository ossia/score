cmake_minimum_required(VERSION 3.0)
project(ScenarioTests LANGUAGES CXX)
enable_testing()

include_directories("${ISCORE_ROOT_SOURCE_DIR}/base")

find_package(Qt5 5.3 REQUIRED COMPONENTS Core Test)

function(addIScoreQtTest TESTNAME TESTSRCS)
                add_executable(${TESTNAME} ${TESTSRCS})
                target_link_libraries(${TESTNAME} PRIVATE Qt5::Core Qt5::Test iscore_lib_base scenario_process DeviceExplorerLibrary StateLibrary)
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
addIScoreQtTest(AddRackToConstraintTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddRackToConstraintTest.cpp")
addIScoreQtTest(AddSlotToRackTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddSlotToRackTest.cpp")
addIScoreQtTest(AddLayerModelToSlotTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddLayerModelToSlotTest.cpp")
addIscoreQtTest(SplitTimeNodeTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/SplitTimeNodeTest.cpp")
addIscoreQtTest(MergeTimeNodesTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MergeTimeNodesTest.cpp")

## elementCreation
addIScoreQtTest(CreateEventAfterEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventAfterEventTest.cpp")
addIScoreQtTest(CreateEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventTest.cpp")
addIScoreQtTest(CreateEventAfterEventOnTimeNodeTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventAfterEventOnTimeNodeTest.cpp")
addIScoreQtTest(CreateConstraintTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateConstraintTest.cpp")

## elementDeletion
addIscoreQtTest(RemoveEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementDeletion/RemoveEventTest.cpp")
addIscoreQtTest(RemoveConstraintTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementDeletion/RemoveConstraintTest.cpp")

addIScoreQtTest(HideRackInViewModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/HideRackInViewModelTest.cpp")
addIScoreQtTest(ShowRackInViewModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ShowRackInViewModelTest.cpp")

addIScoreQtTest(MoveEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveEventTest.cpp")
addIScoreQtTest(MoveConstraintTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveConstraintTest.cpp")

addIScoreQtTest(RemoveRackFromConstraintTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveRackFromConstraintTest.cpp")
addIScoreQtTest(RemoveSlotFromRackTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveSlotFromRackTest.cpp")
addIScoreQtTest(RemoveProcessFromConstraintTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveProcessFromConstraintTest.cpp")
addIScoreQtTest(RemoveLayerFromSlotTest # TODO Rename in RemoveLayerModel
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveLayerFromSlotTest.cpp")
addIScoreQtTest(ResizeSlotVerticallyTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ResizeSlotVerticallyTest.cpp")

## Constraints
addIScoreQtTest(ConstraintModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Constraint/ConstraintModelTests.cpp")

## TimeNodes
addIScoreQtTest(TimeNodeModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/TimeNode/TimeNodeModelTests.cpp")

