project(ScenarioTests LANGUAGES CXX)
enable_testing()

include_directories("${SCORE_ROOT_SOURCE_DIR}/base")

find_package(Qt5 5.3 REQUIRED COMPONENTS Core Test)

function(addIScoreQtTest TESTNAME TESTSRCS)
                add_executable(${TESTNAME} ${TESTSRCS})
                target_link_libraries(${TESTNAME} PRIVATE Qt5::Core Qt5::Test score_lib_base scenario_process DeviceExplorerLibrary StateLibrary)
                add_test(${TESTNAME}_target ${TESTNAME})
endFunction()

add_custom_target(ScenarioTestHeaders
                                  SOURCES
                                   "${CMAKE_CURRENT_SOURCE_DIR}/helpers/FriendDeclaration.hpp"
                                   "${CMAKE_CURRENT_SOURCE_DIR}/helpers/ForwardDeclaration.hpp"
)

# Commands
addIScoreQtTest(AddProcessToIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddProcessToIntervalTest.cpp")
addIScoreQtTest(AddRackToIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddRackToIntervalTest.cpp")
addIScoreQtTest(AddSlotToRackTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddSlotToRackTest.cpp")
addIScoreQtTest(AddLayerModelToSlotTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddLayerModelToSlotTest.cpp")
addIscoreQtTest(SplitTimeSyncTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/SplitTimeSyncTest.cpp")
addIscoreQtTest(MergeTimeSyncsTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MergeTimeSyncsTest.cpp")

## elementCreation
addIScoreQtTest(CreateEventAfterEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventAfterEventTest.cpp")
addIScoreQtTest(CreateEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventTest.cpp")
addIScoreQtTest(CreateEventAfterEventOnTimeSyncTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventAfterEventOnTimeSyncTest.cpp")
addIScoreQtTest(CreateIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateIntervalTest.cpp")

## elementDeletion
addIscoreQtTest(RemoveEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementDeletion/RemoveEventTest.cpp")
addIscoreQtTest(RemoveIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementDeletion/RemoveIntervalTest.cpp")

addIScoreQtTest(HideRackInViewModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/HideRackInViewModelTest.cpp")
addIScoreQtTest(ShowRackInViewModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ShowRackInViewModelTest.cpp")

addIScoreQtTest(MoveEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveEventTest.cpp")
addIScoreQtTest(MoveIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveIntervalTest.cpp")

addIScoreQtTest(RemoveRackFromIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveRackFromIntervalTest.cpp")
addIScoreQtTest(RemoveSlotFromRackTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveSlotFromRackTest.cpp")
addIScoreQtTest(RemoveProcessFromIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveProcessFromIntervalTest.cpp")
addIScoreQtTest(RemoveLayerFromSlotTest # TODO Rename in RemoveLayerModel
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveLayerFromSlotTest.cpp")
addIScoreQtTest(ResizeSlotVerticallyTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ResizeSlotVerticallyTest.cpp")

## Intervals
addIScoreQtTest(IntervalModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Interval/IntervalModelTests.cpp")

## TimeSyncs
addIScoreQtTest(TimeSyncModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/TimeSync/TimeSyncModelTests.cpp")

