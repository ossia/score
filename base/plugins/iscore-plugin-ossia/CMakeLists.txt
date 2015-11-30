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
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/OSSIA2iscore.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Minuit/MinuitProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Minuit/MinuitDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Minuit/MinuitSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Minuit/MinuitProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSSIADevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/MIDI/MIDIProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/MIDI/MIDIDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/MIDI/MIDIProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/MIDI/MIDISpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/OSSIAApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/ProcessModel/OSSIAProcessModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/ProcessModel/OSSIAProcessModelElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/ProcessModel/TimeProcessWithConstraint.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/iscore2OSSIA.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ContextMenu/PlayContextMenu.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ContextMenu/PlayContextMenuFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/BasicProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/LoopingProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/BaseScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/EventElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/ScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/ProcessElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/StateElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/ProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/AutomationElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/DocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/TimeNodeElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/ConstraintElement.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/ProcessModel/ProcessModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/ProcessModel/ProcessModelElement.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_ossia.hpp"
    )
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/MIDI/MIDIProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/MIDI/MIDIDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/MIDI/MIDIProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/MIDI/MIDISpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Minuit/MinuitSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Minuit/MinuitProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Minuit/MinuitDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Minuit/MinuitProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSSIADevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/OSSIA2iscore.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/OSSIAApplicationPlugin.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ContextMenu/PlayContextMenu.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ContextMenu/PlayContextMenuFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/BasicProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/LoopingProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/BaseScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/EventElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/ScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/StateElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/AutomationElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/DocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/TimeNodeElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/ConstraintElement.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/iscore2OSSIA.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_ossia.cpp"
)

if(TARGET iscore_plugin_mapping)
    set(SRCS ${SRCS}
        "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/MappingElement.cpp"
)
    set(HDRS ${HDRS}
        "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/MappingElement.hpp"
)
endif()

if(TARGET iscore_plugin_loop)
    set(SRCS ${SRCS}
        "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/LoopElement.cpp"
)
    set(HDRS ${HDRS}
        "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/RecreateOnPlayDocumentPlugin/LoopElement.hpp"
)
endif()

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                        Qt5::Core Qt5::Widgets
                        iscore_lib_base iscore_plugin_scenario iscore_plugin_curve iscore_plugin_automation iscore_plugin_deviceexplorer
                        APIJamoma)

if(TARGET iscore_plugin_mapping)
    target_link_libraries(${PROJECT_NAME} iscore_plugin_mapping)
endif()
if(TARGET iscore_plugin_loop)
    target_link_libraries(${PROJECT_NAME} iscore_plugin_loop)
endif()

setup_iscore_plugin(${PROJECT_NAME})
add_subdirectory(SimpleProcess)
