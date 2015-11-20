cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_ossia)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

find_package(Jamoma 0.6)
if(NOT ${Jamoma_FOUND})
    message("Jamoma not found - won't build OSSIA plugin")
    return()
endif()

# Files & main target
set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA2iscore.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSSIADevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDISpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIAControl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_ossia.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ProcessModel/OSSIAProcessModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ProcessModel/OSSIAProcessModelElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ProcessModel/TimeProcessWithConstraint.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/iscore2OSSIA.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/ContextMenu/PlayContextMenu.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/ContextMenu/PlayContextMenuFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/LoopingProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIABaseScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAEventElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAProcessElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAStateElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/BasicProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/ProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAAutomationElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIADocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIATimeNodeElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAConstraintElement.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/BasicProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/LoopingProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/BaseScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/EventElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/ScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/ProcessElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/StateElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/ProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/AutomationElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/DocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/TimeNodeElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/ConstraintElement.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/ProcessModel/ProcessModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/ProcessModel/ProcessModelElement.hpp"
    )
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDISpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSSIADevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_ossia.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA2iscore.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIAControl.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/ContextMenu/PlayContextMenu.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/ContextMenu/PlayContextMenuFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/BasicProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/LoopingProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAStateElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIATimeNodeElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAAutomationElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIABaseScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIADocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAConstraintElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAEventElement.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/BasicProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/LoopingProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/BaseScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/EventElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/ScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/StateElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/AutomationElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/DocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/TimeNodeElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RecreateOnPlayDocumentPlugin/ConstraintElement.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore2OSSIA.cpp"
)

if(TARGET iscore_plugin_mapping)
    set(SRCS ${SRCS}
        "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAMappingElement.cpp")
    set(HDRS ${HDRS}
        "${CMAKE_CURRENT_SOURCE_DIR}/DocumentPlugin/OSSIAMappingElement.hpp")
endif()


add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                        Qt5::Core Qt5::Widgets
                        iscore_lib_base iscore_plugin_scenario iscore_plugin_curve iscore_plugin_automation iscore_plugin_deviceexplorer
                        APIJamoma)

if(TARGET iscore_plugin_mapping)
    target_link_libraries(${PROJECT_NAME} iscore_plugin_mapping)
endif()

setup_iscore_plugin(${PROJECT_NAME})
add_subdirectory(SimpleProcess)
