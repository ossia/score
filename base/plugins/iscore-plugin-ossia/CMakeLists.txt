cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_ossia)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Xml PrintSupport Test)

find_package(Jamoma 0.6)
if(NOT ${Jamoma_FOUND})
    message("Jamoma not found - won't build OSSIA plugin")
    return()
endif()

# Files & main target
file(GLOB HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/ContextMenu/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ProcessModel/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/*.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDISpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSSIADevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_ossia.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA2iscore.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIAControl.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/BasicProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/LoopingProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAStateElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIATimeNodeElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAAutomationElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAMappingElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIABaseScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/ContextMenu/PlayContextMenu.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/ContextMenu/PlayContextMenuFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIADocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAConstraintElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAEventElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore2OSSIA.cpp"
)


add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                        Qt5::Core Qt5::Widgets
                        iscore_lib_base iscore_plugin_scenario iscore_plugin_curve iscore_plugin_automation iscore_plugin_mapping iscore_plugin_deviceexplorer
                        OSSIAHeadersLib APIJamoma)

setup_iscore_plugin(${PROJECT_NAME})


add_subdirectory(SimpleProcess)
