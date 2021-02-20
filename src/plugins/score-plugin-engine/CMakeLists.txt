project(score_plugin_engine LANGUAGES CXX)

score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets Network)

# Files & main target
set(HDRS

  Execution/ContextMenu/PlayContextMenu.hpp
  Execution/ContextMenu/PlayFromIntervalInScenario.hpp

  Execution/BaseScenarioComponent.hpp
  Execution/DocumentPlugin.hpp
  Execution/ExecutionTick.hpp

  Execution/Automation/InterpStateComponent.hpp

  Execution/Settings/ExecutorModel.hpp
  Execution/Settings/ExecutorPresenter.hpp
  Execution/Settings/ExecutorView.hpp
  Execution/Settings/ExecutorFactory.hpp

  Execution/Clock/ClockFactory.hpp
  Execution/Clock/ManualClock.hpp
  Execution/Clock/DefaultClock.hpp
  Engine/ApplicationPlugin.hpp
  Engine/Listening/PlayListeningHandler.hpp
  Engine/Listening/PlayListeningHandlerFactory.hpp

  Engine/Node/PdNode.hpp
  Engine/Node/Process.hpp
  Engine/Node/Inspector.hpp
  Engine/Node/Layer.hpp
  Engine/Node/Executor.hpp
  Engine/Node/CommonWidgets.hpp
  Engine/Node/Port.hpp
  Engine/Node/TickPolicy.hpp

  LocalTree/Device/LocalDevice.hpp
  LocalTree/Device/LocalProtocolSettingsWidget.hpp
  LocalTree/Device/LocalProtocolFactory.hpp
  LocalTree/Device/LocalSpecificSettings.hpp
  LocalTree/LocalTreeDocumentPlugin.hpp

  score_plugin_engine.hpp
)

set(SRCS
  Engine/ApplicationPlugin.cpp

  Execution/ContextMenu/PlayContextMenu.cpp
  Execution/ContextMenu/PlayFromIntervalInScenario.cpp

  Execution/BaseScenarioComponent.cpp
  Execution/DocumentPlugin.cpp
  Execution/ExecutionTick.cpp

  Execution/Automation/InterpStateComponent.cpp
  Execution/Clock/ClockFactory.cpp
  Execution/Clock/DefaultClock.cpp

  Execution/Settings/ExecutorModel.cpp
  Execution/Settings/ExecutorPresenter.cpp
  Execution/Settings/ExecutorView.cpp

  Engine/Listening/PlayListeningHandler.cpp
  Engine/Listening/PlayListeningHandlerFactory.cpp

  LocalTree/Device/LocalProtocolSettingsWidget.cpp
  LocalTree/Device/LocalSpecificSettingsSerialization.cpp
  LocalTree/Device/LocalDevice.cpp
  LocalTree/Device/LocalProtocolFactory.cpp
  LocalTree/LocalTreeDocumentPlugin.cpp

  score_plugin_engine.cpp
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${MAPPER_SRCS})

target_link_libraries(${PROJECT_NAME}
        PUBLIC
          ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets
          score_lib_base score_lib_device score_lib_process
          score_plugin_library score_plugin_deviceexplorer
          score_plugin_scenario
          score_plugin_audio
          ossia
)

if(OSSIA_PROTOCOL_AUDIO)
  target_sources(
    ${PROJECT_NAME}
    PRIVATE
      Execution/Clock/DataflowClock.hpp
      Execution/Clock/DataflowClock.cpp
    )
endif()
setup_score_plugin(${PROJECT_NAME})
