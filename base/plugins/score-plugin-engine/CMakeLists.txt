cmake_minimum_required(VERSION 3.1)
project(score_plugin_engine LANGUAGES CXX)

score_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)
find_package(TBB)
# Files & main target
set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/CurveConversion.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/OSSIA2score.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/DefaultProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/OSC/OSCSpecificSettings.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Settings/Model.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Settings/Presenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Settings/View.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Settings/Factory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Panel/MessagesPanel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Curve/EasingSegment.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/score2OSSIA.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ContextMenu/PlayContextMenu.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ContextMenu/PlayFromIntervalInScenario.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Component.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ExecutorContext.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/BaseScenarioComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/EventComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ScenarioComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ProcessComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/StateComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/DocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/TimeSyncComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/TimeSyncRawPtrComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/IntervalComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/IntervalRawPtrComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/Component.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/SplineComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/InterpolationComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/InterpStateComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/GradientComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/MetronomeComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Mapping/Component.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Loop/Component.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ClockManager/ClockManagerFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ClockManager/DefaultClockManager.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Listening/PlayListeningHandler.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Listening/PlayListeningHandlerFactory.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/LocalTreeComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/LocalTreeDocumentPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/BaseProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Property.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/SetProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/GetProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/NameProperty.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/BaseCallbackWrapper.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/AutomationComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/MappingComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/ProcessComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/ScenarioComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/LoopComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/IntervalComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/EventComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/TimeSyncComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/StateComponent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/MetadataParameters.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/PdNode.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/Widgets.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/Process.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/Inspector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/Layer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/Executor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/BaseWidgets.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/CommonWidgets.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/Port.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/TimeSignature.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/TickPolicy.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/AudioPanel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_engine.hpp"
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

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Settings/Model.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Settings/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Settings/View.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Panel/MessagesPanel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/OSSIA2score.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/ApplicationPlugin.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ContextMenu/PlayContextMenu.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ContextMenu/PlayFromIntervalInScenario.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/BaseScenarioComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/EventComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ScenarioComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/StateComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/DocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/TimeSyncComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/IntervalComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/TimeSyncRawPtrComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/IntervalRawPtrComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/Component.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/GradientComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/MetronomeComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/SplineComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/InterpolationComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Automation/InterpStateComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Mapping/Component.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Loop/Component.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ClockManager/ClockManagerFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/ClockManager/DefaultClockManager.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Settings/ExecutorView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Listening/PlayListeningHandler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Listening/PlayListeningHandlerFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/LocalTreeDocumentPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/BaseProperty.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/LTProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/LTScenarioComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/LTLoopComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/LTIntervalComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/LTEventComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/LTTimeSyncComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/LTStateComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/LocalTree/Scenario/MetadataParameters.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Engine/score2OSSIA.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/AudioPanel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Engine/Node/Widgets.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_engine.cpp"
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

set(PHIDGETS_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Phidgets/PhidgetsProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Phidgets/PhidgetsDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Phidgets/PhidgetsProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Phidgets/PhidgetsSpecificSettings.hpp"
)
set(PHIDGETS_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Phidgets/PhidgetsProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Phidgets/PhidgetsDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Phidgets/PhidgetsProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Phidgets/PhidgetsSpecificSettingsSerialization.cpp"
)

set(AUDIO_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Audio/AudioDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Dataflow/DataflowClock.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Dataflow/ManualClock.hpp"
)
set(AUDIO_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Protocols/Audio/AudioDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Executor/Dataflow/DataflowClock.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

if(OSSIA_PROTOCOL_MIDI)
    target_sources(${PROJECT_NAME} PRIVATE ${MIDI_HDRS} ${MIDI_SRCS})
endif()

if(OSSIA_PROTOCOL_HTTP)
    target_sources(${PROJECT_NAME} PRIVATE ${HTTP_HDRS} ${HTTP_SRCS})
endif()

if(OSSIA_PROTOCOL_WEBSOCKETS)
    target_sources(${PROJECT_NAME} PRIVATE ${WS_HDRS} ${WS_SRCS})
endif()

if(OSSIA_PROTOCOL_SERIAL)
    target_sources(${PROJECT_NAME} PRIVATE ${SERIAL_HDRS} ${SERIAL_SRCS})
    find_package(Qt5 5.7 REQUIRED COMPONENTS SerialPort)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::SerialPort)
endif()

if(OSSIA_PROTOCOL_PHIDGETS)
    target_sources(${PROJECT_NAME} PRIVATE ${PHIDGETS_HDRS} ${PHIDGETS_SRCS})
endif()
if(OSSIA_DATAFLOW)
    target_sources(${PROJECT_NAME} PRIVATE ${AUDIO_HDRS} ${AUDIO_SRCS})
endif()

score_moc_headers(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_engine.hpp")
target_link_libraries(${PROJECT_NAME}
        PUBLIC
          Qt5::Core Qt5::Widgets
          score_lib_base score_lib_device score_lib_process
          score_plugin_scenario score_plugin_curve score_plugin_automation score_plugin_deviceexplorer
          score_plugin_mapping
          score_plugin_loop
          ossia
)

setup_score_plugin(${PROJECT_NAME})
