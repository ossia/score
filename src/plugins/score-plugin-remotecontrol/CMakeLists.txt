project(score_plugin_remotecontrol LANGUAGES CXX)

if(SCORE_FAST_DEV_BUILD)
  return()
endif()

if(NOT TARGET "${QT_PREFIX}::WebSockets")
  message("Remote control plug-in requires QtWebSockets.")
  return()
endif()

if(NOT TARGET score_plugin_js)
  message(FATAL_ERROR "Remote control plug-in requires score_plugin_js.")
  return()
endif()
# General initialization
score_common_setup()

# Source files
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/Event.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/State.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/Sync.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/Process.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/Scenario.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/Interval.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/DocumentPlugin.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Controller/RemoteControlProvider.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Controller/DocumentPlugin.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/Model.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/View.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/Presenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/Factory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/ApplicationPlugin.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/i-score-remote/RemoteApplication.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_remotecontrol.hpp"
  )
set(SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Controller/RemoteControlProvider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Controller/DocumentPlugin.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/Model.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/View.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/Process.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/Scenario.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/Interval.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/Event.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/Sync.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/Scenario/State.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Websockets/DocumentPlugin.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/ApplicationPlugin.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_remotecontrol.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${QRCS})

target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_scenario score_plugin_js ${QT_PREFIX}::WebSockets)

setup_score_plugin(${PROJECT_NAME})

### Setup of the target ###
#TODO
#function(setup_score_addon AddonTarget)
#    set(options)
#    set(oneValueArgs NAME METADATA)
#    set(multiValueArgs)
#    cmake_parse_arguments(SETUP_ADDON "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

#    setup_score_common_lib_features("${AddonTarget}")

#    set(ADDON_FOLDER "${CMAKE_BINARY_DIR}/addons/${SETUP_PLUGIN_NAME}/")
#    set(ADDON_PLATFORM "${SCORE_PLUGIN_PLATFORM}")
#    set(ADDON_FILENAME "${SETUP_PLUGIN_NAME}-${SCORE_PLUGIN_SUFFIX}")

#    set_target_properties(${AddonTarget} PROPERTIES
#        LIBRARY_OUTPUT_DIRECTORY "${ADDON_FOLDER}/"
#        PREFIX ""
#        SUFFIX ""
#        OUTPUT_NAME "${ADDON_FILENAME}")
#    configure_file("${SETUP_PLUGIN_METADATA}" "${ADDON_FOLDER}/localaddon.json")

#endfunction()

#setup_score_addon(
#    ${PROJECT_NAME}
#    NAME RemoteControl
#    METADATA "${CMAKE_CURRENT_SOURCE_DIR}/Deployment/localaddon.json.in"
#     )
