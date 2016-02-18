cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_ossia LANGUAGES CXX)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/CurveConversion.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/iscore2OSSIA.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ContextMenu/PlayContextMenu.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ContextMenu/PlayContextMenuFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ExecutorContext.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/BasicProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/LoopingProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/BaseScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/EventElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ProcessElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/StateElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ProcessWrapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/DocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/TimeNodeElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ConstraintElement.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/Settings/Model.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/Settings/Presenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/Settings/View.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/Settings/Factory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Listening/PlayListeningHandler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Listening/PlayListeningHandlerFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/BaseProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Property.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/SetProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/GetProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/NameProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/BaseCallbackWrapper.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/ProcessComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/ScenarioComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/ScenarioComponentFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/ConstraintComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/EventComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/TimeNodeComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/StateComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/MetadataParameters.hpp"

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

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ContextMenu/PlayContextMenu.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ContextMenu/PlayContextMenuFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/BasicProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/LoopingProcessWrapper.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/BaseScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/EventElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/StateElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/DocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/TimeNodeElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/ConstraintElement.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/Settings/Model.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/Settings/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/Settings/View.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Executor/Settings/Factory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Listening/PlayListeningHandler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/Listening/PlayListeningHandlerFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/LocalTreeDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/BaseProperty.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/ProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/ScenarioComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/ScenarioComponentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/ConstraintComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/EventComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/TimeNodeComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/StateComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/LocalTree/Scenario/MetadataParameters.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/OSSIA/iscore2OSSIA.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_ossia.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
        PUBLIC  APIJamoma Jamoma::Foundation Jamoma::Modular
				Qt5::Core Qt5::Widgets
                iscore_lib_base iscore_lib_device iscore_lib_process iscore_plugin_scenario iscore_plugin_curve iscore_plugin_automation iscore_plugin_deviceexplorer
)

if(TARGET iscore_plugin_loop)
	target_link_libraries(${PROJECT_NAME} PUBLIC iscore_plugin_loop)
endif()

setup_iscore_plugin(${PROJECT_NAME})
add_subdirectory(SimpleProcess)
