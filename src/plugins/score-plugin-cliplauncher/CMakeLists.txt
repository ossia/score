project(score_plugin_cliplauncher LANGUAGES CXX)

# General initialization
score_common_setup()

# Source files
set(HDRS
    ClipLauncher/Types.hpp
    ClipLauncher/TransitionRule.hpp
    ClipLauncher/CommandFactory.hpp
    ClipLauncher/Metadata.hpp

    ClipLauncher/LaneModel.hpp
    ClipLauncher/SceneModel.hpp
    ClipLauncher/CellModel.hpp
    ClipLauncher/ProcessModel.hpp

    ClipLauncher/Commands/AddLane.hpp
    ClipLauncher/Commands/RemoveLane.hpp
    ClipLauncher/Commands/AddScene.hpp
    ClipLauncher/Commands/RemoveScene.hpp
    ClipLauncher/Commands/AddCell.hpp
    ClipLauncher/Commands/RemoveCell.hpp
    ClipLauncher/Commands/MoveCell.hpp
    ClipLauncher/Commands/CellTriggerCommandFactory.hpp
    ClipLauncher/Commands/SetCellProperties.hpp
    ClipLauncher/Commands/SetLaneProperties.hpp
    ClipLauncher/Commands/SetSceneProperties.hpp
    ClipLauncher/Commands/AddTransitionRule.hpp
    ClipLauncher/Commands/RemoveTransitionRule.hpp

    ClipLauncher/Execution/ClipLauncherComponent.hpp

    ClipLauncher/Inspector/CellInspectorWidget.hpp
    ClipLauncher/Inspector/CellInspectorFactory.hpp
    ClipLauncher/Inspector/LaneInspectorWidget.hpp
    ClipLauncher/Inspector/LaneInspectorFactory.hpp
    ClipLauncher/Inspector/SceneInspectorWidget.hpp
    ClipLauncher/Inspector/SceneInspectorFactory.hpp

    ClipLauncher/View/ClipLauncherView.hpp
    ClipLauncher/View/ClipLauncherPresenter.hpp
    ClipLauncher/View/CellDisplayedElementsProvider.hpp
    ClipLauncher/View/LayerFactory.hpp

    score_plugin_cliplauncher.hpp
)

set(SRCS
    ClipLauncher/LaneModel.cpp
    ClipLauncher/SceneModel.cpp
    ClipLauncher/CellModel.cpp
    ClipLauncher/ProcessModel.cpp

    ClipLauncher/Commands/AddLane.cpp
    ClipLauncher/Commands/RemoveLane.cpp
    ClipLauncher/Commands/AddScene.cpp
    ClipLauncher/Commands/RemoveScene.cpp
    ClipLauncher/Commands/AddCell.cpp
    ClipLauncher/Commands/RemoveCell.cpp
    ClipLauncher/Commands/MoveCell.cpp
    ClipLauncher/Commands/CellTriggerCommandFactory.cpp
    ClipLauncher/Commands/SetCellProperties.cpp
    ClipLauncher/Commands/SetLaneProperties.cpp
    ClipLauncher/Commands/SetSceneProperties.cpp
    ClipLauncher/Commands/AddTransitionRule.cpp
    ClipLauncher/Commands/RemoveTransitionRule.cpp

    ClipLauncher/Execution/ClipLauncherComponent.cpp

    ClipLauncher/Inspector/CellInspectorWidget.cpp
    ClipLauncher/Inspector/CellInspectorFactory.cpp
    ClipLauncher/Inspector/LaneInspectorWidget.cpp
    ClipLauncher/Inspector/LaneInspectorFactory.cpp
    ClipLauncher/Inspector/SceneInspectorWidget.cpp
    ClipLauncher/Inspector/SceneInspectorFactory.cpp

    ClipLauncher/View/ClipLauncherView.cpp
    ClipLauncher/View/ClipLauncherPresenter.cpp
    ClipLauncher/View/CellDisplayedElementsProvider.cpp
    ClipLauncher/View/LayerFactory.cpp

    score_plugin_cliplauncher.cpp
)

# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

# Link
target_link_libraries(${PROJECT_NAME}
  PUBLIC
    score_plugin_scenario
    score_plugin_engine
    score_lib_inspector
)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
