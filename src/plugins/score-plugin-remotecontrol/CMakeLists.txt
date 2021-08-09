project(score_plugin_remotecontrol LANGUAGES CXX)

find_package(${QT_VERSION} OPTIONAL_COMPONENTS WebSockets)
if(NOT TARGET ${QT_PREFIX}::WebSockets)
  message("Remote control plug-in requires QtWebSockets.")
  return()
endif()

# General initialization
score_common_setup()

# Source files
set(HDRS
  "RemoteControl/Scenario/Event.hpp"
  "RemoteControl/Scenario/State.hpp"
  "RemoteControl/Scenario/Sync.hpp"
  "RemoteControl/Scenario/Process.hpp"
  "RemoteControl/Scenario/Scenario.hpp"
  "RemoteControl/Scenario/Interval.hpp"
  "RemoteControl/Settings/Model.hpp"
  "RemoteControl/Settings/View.hpp"
  "RemoteControl/Settings/Presenter.hpp"
  "RemoteControl/Settings/Factory.hpp"
  "RemoteControl/ApplicationPlugin.hpp"
  "RemoteControl/DocumentPlugin.hpp"
  "i-score-remote/RemoteApplication.hpp"
  "score_plugin_remotecontrol.hpp"
  "RemoteControl/Http_server.hpp"
  )
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/Model.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Settings/View.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Process.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Scenario.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Interval.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Event.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/Sync.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Scenario/State.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/ApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/DocumentPlugin.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/Http_server.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_remotecontrol.cpp"
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/RemoteControl/build-wasm/ DESTINATION ${CMAKE_BINARY_DIR}/src/plugins/score-plugin-remotecontrol/CMakeFiles/score_plugin_remotecontrol.dir/RemoteControl/build-wasm/ )

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${QRCS})

target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_scenario score_plugin_js ${QT_PREFIX}::WebSockets ossia)

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
