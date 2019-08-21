cmake_minimum_required(VERSION 3.1)
project(score_addon_remotecontrol LANGUAGES CXX)

find_package(Qt5 5.12 REQUIRED COMPONENTS WebSockets)
###  General initialization of always necessary stuff (AUTOMOC, etc) ###
score_common_setup()

### Files & main target ###
set(HDRS
  "RemoteControl/Scenario/Event.hpp"
  "RemoteControl/Scenario/State.hpp"
  "RemoteControl/Scenario/Sync.hpp"
  "RemoteControl/Scenario/Process.hpp"
  "RemoteControl/Scenario/Loop.hpp"
  "RemoteControl/Scenario/Scenario.hpp"
  "RemoteControl/Scenario/Interval.hpp"
  "RemoteControl/Settings/Model.hpp"
  "RemoteControl/Settings/View.hpp"
  "RemoteControl/Settings/Presenter.hpp"
  "RemoteControl/Settings/Factory.hpp"
  "RemoteControl/ApplicationPlugin.hpp"
  "RemoteControl/DocumentPlugin.hpp"
  "i-score-remote/RemoteApplication.hpp"
  "score_addon_remotecontrol.hpp"
  )
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/Model.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/View.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Process.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Scenario.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Loop.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Interval.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Event.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Sync.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/State.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/ApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/DocumentPlugin.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_addon_remotecontrol.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${QRCS})

target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_scenario score_plugin_loop Qt5::WebSockets)

setup_score_plugin(${PROJECT_NAME})

### Setup of the target ###
#TODO
#function(setup_score_addon AddonTarget)
#    set(options)
#    set(oneValueArgs NAME METADATA)
#    set(multiValueArgs)
#    cmake_parse_arguments(SETUP_ADDON "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

#    setup_score_common_lib_features("${AddonTarget}")

#    set(ADDON_FOLDER "${CMAKE_BINARY_DIR}/addons/${SETUP_ADDON_NAME}/")
#    set(ADDON_PLATFORM "${SCORE_ADDON_PLATFORM}")
#    set(ADDON_FILENAME "${SETUP_ADDON_NAME}-${SCORE_ADDON_SUFFIX}")

#    set_target_properties(${AddonTarget} PROPERTIES
#        LIBRARY_OUTPUT_DIRECTORY "${ADDON_FOLDER}/"
#        PREFIX ""
#        SUFFIX ""
#        OUTPUT_NAME "${ADDON_FILENAME}")
#    configure_file("${SETUP_ADDON_METADATA}" "${ADDON_FOLDER}/localaddon.json")

#    score_cotire_post("${AddonTarget}")
#endfunction()

#setup_score_addon(
#    ${PROJECT_NAME}
#    NAME RemoteControl
#    METADATA "${CMAKE_CURRENT_SOURCE_DIR}/Deployment/localaddon.json.in"
#     )
