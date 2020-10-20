project(ScenarioTests LANGUAGES CXX)
enable_testing()

include_directories("${SCORE_ROOT_SOURCE_DIR}/base")

find_package(${QT_VERSION} REQUIRED COMPONENTS Core Test)

function(addScoreQtTest TESTNAME TESTSRCS)
                add_executable(${TESTNAME} ${TESTSRCS})
                target_link_libraries(${TESTNAME} PRIVATE ${QT_PREFIX}::Core ${QT_PREFIX}::Test score_lib_base scenario_process DeviceExplorerLibrary StateLibrary)
                add_test(${TESTNAME}_target ${TESTNAME})
endFunction()

add_custom_target(ScenarioTestHeaders
                                  SOURCES
                                   "${CMAKE_CURRENT_SOURCE_DIR}/helpers/FriendDeclaration.hpp"
                                   "${CMAKE_CURRENT_SOURCE_DIR}/helpers/ForwardDeclaration.hpp"
)

# Commands
addScoreQtTest(AddProcessToIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddProcessToIntervalTest.cpp")
addScoreQtTest(AddRackToIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddRackToIntervalTest.cpp")
addScoreQtTest(AddSlotToRackTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddSlotToRackTest.cpp")
addScoreQtTest(AddLayerModelToSlotTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/AddLayerModelToSlotTest.cpp")
addScoreQtTest(SplitTimeSyncTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/SplitTimeSyncTest.cpp")
addScoreQtTest(MergeTimeSyncsTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MergeTimeSyncsTest.cpp")

## elementCreation
addScoreQtTest(CreateEventAfterEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventAfterEventTest.cpp")
addScoreQtTest(CreateEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventTest.cpp")
addScoreQtTest(CreateEventAfterEventOnTimeSyncTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateEventAfterEventOnTimeSyncTest.cpp")
addScoreQtTest(CreateIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementCreation/CreateIntervalTest.cpp")

## elementDeletion
addScoreQtTest(RemoveEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementDeletion/RemoveEventTest.cpp")
addScoreQtTest(RemoveIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ElementDeletion/RemoveIntervalTest.cpp")

addScoreQtTest(HideRackInViewModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/HideRackInViewModelTest.cpp")
addScoreQtTest(ShowRackInViewModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ShowRackInViewModelTest.cpp")

addScoreQtTest(MoveEventTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveEventTest.cpp")
addScoreQtTest(MoveIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/MoveIntervalTest.cpp")

addScoreQtTest(RemoveRackFromIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveRackFromIntervalTest.cpp")
addScoreQtTest(RemoveSlotFromRackTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveSlotFromRackTest.cpp")
addScoreQtTest(RemoveProcessFromIntervalTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveProcessFromIntervalTest.cpp")
addScoreQtTest(RemoveLayerFromSlotTest # TODO Rename in RemoveLayerModel
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/RemoveLayerFromSlotTest.cpp")
addScoreQtTest(ResizeSlotVerticallyTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Commands/ResizeSlotVerticallyTest.cpp")

## Intervals
addScoreQtTest(IntervalModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/Interval/IntervalModelTests.cpp")

## TimeSyncs
addScoreQtTest(TimeSyncModelTest
                                                                "${CMAKE_CURRENT_SOURCE_DIR}/TimeSync/TimeSyncModelTests.cpp")

