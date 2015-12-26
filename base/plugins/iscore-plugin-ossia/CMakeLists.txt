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
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Local/LocalProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Local/LocalDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Local/LocalSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Local/LocalProtocolFactory.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/ProcessModel/TimeProcessWithConstraint.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/iscore2OSSIA.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ContextMenu/PlayContextMenu.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ContextMenu/PlayContextMenuFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/BasicProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/LoopingProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/BaseScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/EventElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ProcessElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/StateElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/AutomationElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/DocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/TimeNodeElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ConstraintElement.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ProcessModel/ProcessModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ProcessModel/ProcessModelElement.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Property.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/SetProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/GetProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/BaseCallbackWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/ScenarioTreeBuilder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/ProcessComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/ScenarioComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/ConstraintComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/EventComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/TimeNodeComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/StateComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/MetadataParameters.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/BaseProperty.hpp"

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
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Local/LocalSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Local/LocalProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Local/LocalDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/Local/LocalProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSC/OSCProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Protocols/OSSIADevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/OSSIA2iscore.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/OSSIAApplicationPlugin.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ContextMenu/PlayContextMenu.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ContextMenu/PlayContextMenuFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/BasicProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/LoopingProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/BaseScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/EventElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/StateElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/AutomationElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/DocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/TimeNodeElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/ConstraintElement.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/LocalTreeDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/ScenarioTreeBuilder.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/ProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/ScenarioComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/ConstraintComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/EventComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/TimeNodeComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/StateComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/MetadataParameters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/BaseProperty.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/iscore2OSSIA.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_ossia.cpp"
)

if(TARGET iscore_plugin_mapping)
    set(SRCS ${SRCS}
        "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/MappingElement.cpp"
)
    set(HDRS ${HDRS}
        "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/MappingElement.hpp"
)
endif()

if(TARGET iscore_plugin_loop)
    set(SRCS ${SRCS}
        "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/LoopElement.cpp"
)
    set(HDRS ${HDRS}
        "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/DocumentPlugin/LoopElement.hpp"
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
