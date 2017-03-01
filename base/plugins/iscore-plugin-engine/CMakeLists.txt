cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_engine LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/CurveConversion.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/OSSIA2iscore.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/OssiaLogger.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSCQuery/OSCQueryProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSCQuery/OSCQueryDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSCQuery/OSCQuerySpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSCQuery/OSCQueryProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Local/LocalProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSSIADevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCSpecificSettings.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Panel/MessagesPanel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Curve/EasingSegment.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/iscore2OSSIA.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ContextMenu/PlayContextMenu.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ContextMenu/PlayFromConstraintInScenario.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ExecutorContext.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/BaseScenarioComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/EventComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ScenarioComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ProcessComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/StateProcessComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/StateComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/DocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/TimeNodeComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ConstraintComponent.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/LoopComponent.hpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitSpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Minuit/MinuitProtocolFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSCQuery/OSCQuerySpecificSettingsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSCQuery/OSCQueryProtocolSettingsWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSCQuery/OSCQueryDevice.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSCQuery/OSCQueryProtocolFactory.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ContextMenu/PlayFromConstraintInScenario.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/StateProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/BaseScenarioComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/EventComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ScenarioComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/StateComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/DocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/TimeNodeComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ConstraintComponent.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/LoopComponent.cpp"
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

set(MIDI_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDISpecificSettings.hpp"
)
set(MIDI_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDIProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/MIDI/MIDISpecificSettingsSerialization.cpp"
)

set(HTTP_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/HTTP/HTTPProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/HTTP/HTTPDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/HTTP/HTTPProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/HTTP/HTTPSpecificSettings.hpp"
)
set(HTTP_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/HTTP/HTTPProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/HTTP/HTTPDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/HTTP/HTTPProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/HTTP/HTTPSpecificSettingsSerialization.cpp"
)

set(WS_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/WS/WSProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/WS/WSDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/WS/WSProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/WS/WSSpecificSettings.hpp"
)
set(WS_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/WS/WSProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/WS/WSDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/WS/WSProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/WS/WSSpecificSettingsSerialization.cpp"
)

set(SERIAL_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Serial/SerialProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Serial/SerialDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Serial/SerialProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Serial/SerialSpecificSettings.hpp"
)
set(SERIAL_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Serial/SerialProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Serial/SerialDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Serial/SerialProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Serial/SerialSpecificSettingsSerialization.cpp"
)
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

get_target_property(OSSIA_PROTOCOLS ossia OSSIA_PROTOCOLS)
if("${OSSIA_PROTOCOLS}" MATCHES "MIDI")
    target_sources(${PROJECT_NAME} PRIVATE ${MIDI_HDRS} ${MIDI_SRCS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE OSSIA_PROTOCOL_MIDI)
endif()

if("${OSSIA_PROTOCOLS}" MATCHES "HTTP")
    target_sources(${PROJECT_NAME} PRIVATE ${HTTP_HDRS} ${HTTP_SRCS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE OSSIA_PROTOCOL_HTTP)
endif()

if("${OSSIA_PROTOCOLS}" MATCHES "WebSockets")
    target_sources(${PROJECT_NAME} PRIVATE ${WS_HDRS} ${WS_SRCS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE OSSIA_PROTOCOL_WEBSOCKETS)
endif()

if("${OSSIA_PROTOCOLS}" MATCHES "Serial")
    target_sources(${PROJECT_NAME} PRIVATE ${SERIAL_HDRS} ${SERIAL_SRCS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE OSSIA_PROTOCOL_SERIAL)
endif()

target_link_libraries(${PROJECT_NAME}
        PUBLIC
          Qt5::Core Qt5::Widgets
          iscore_lib_base iscore_lib_device iscore_lib_process iscore_plugin_scenario iscore_plugin_curve iscore_plugin_automation iscore_plugin_deviceexplorer
          ossia
)

target_link_libraries(${PROJECT_NAME} PUBLIC iscore_plugin_loop)
setup_iscore_plugin(${PROJECT_NAME})
