cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_engine LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/CurveConversion.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/OSSIA2iscore.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSSIADevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDISpecificSettings.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Panel/MessagesPanel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Curve/EasingSegment.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/iscore2OSSIA.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ContextMenu/PlayContextMenu.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ExecutorContext.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/BaseScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/EventElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ScenarioElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ProcessElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/StateProcessComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/StateElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/DocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/TimeNodeElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ConstraintElement.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Interpolation/InterpolationComponent.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ClockManager/ClockManagerFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ClockManager/DefaultClockManager.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Listening/PlayListeningHandler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Listening/PlayListeningHandlerFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/LocalTreeDocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/BaseProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Property.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/SetProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/GetProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/NameProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/BaseCallbackWrapper.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/ProcessComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/ScenarioComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/ConstraintComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/EventComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/TimeNodeComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/StateComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/MetadataParameters.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Settings/LocalTreeModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Settings/LocalTreePresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Settings/LocalTreeView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Settings/LocalTreeFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_engine.hpp"
    )
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDISpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSSIADevice.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Panel/MessagesPanel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/OSSIA2iscore.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/ApplicationPlugin.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ContextMenu/PlayContextMenu.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/StateProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/BaseScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/EventElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ScenarioElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/StateElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/DocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/TimeNodeElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ConstraintElement.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Interpolation/InterpolationComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ClockManager/ClockManagerFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ClockManager/DefaultClockManager.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Listening/PlayListeningHandler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Listening/PlayListeningHandlerFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/LocalTreeDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/BaseProperty.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/ProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/ScenarioComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/ConstraintComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/EventComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/TimeNodeComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/StateComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/MetadataParameters.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Settings/LocalTreeModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Settings/LocalTreePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Settings/LocalTreeView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/iscore2OSSIA.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_engine.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

target_link_libraries(${PROJECT_NAME}
        PUBLIC
          Qt5::Core Qt5::Widgets
          iscore_lib_base iscore_lib_device iscore_lib_process iscore_plugin_scenario iscore_plugin_curve iscore_plugin_automation iscore_plugin_deviceexplorer
          ossia
)

target_link_libraries(${PROJECT_NAME} PUBLIC iscore_plugin_loop)
setup_iscore_plugin(${PROJECT_NAME})
