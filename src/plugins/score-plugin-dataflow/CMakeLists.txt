project(score_plugin_dataflow LANGUAGES CXX)

# General initialization
score_common_setup()

# Source files
set(HDRS
    Dataflow/PortItem.hpp
    Dataflow/CableInspector.hpp
    Dataflow/ControlInletItem.hpp
    Dataflow/ControlOutletItem.hpp
    Dataflow/MidiInletItem.hpp
    Dataflow/MidiOutletItem.hpp
    Dataflow/AudioInletItem.hpp
    Dataflow/AudioOutletItem.hpp
    Dataflow/PortInspectorFactory.hpp
    Dataflow/ValueInletItem.hpp
    Dataflow/ValueOutletItem.hpp
    Dataflow/DropPortInScenario.hpp
    Dataflow/WidgetInletFactory.hpp
    score_plugin_dataflow.hpp
)
set(SRCS
    Dataflow/PortItem.cpp
    Dataflow/ControlInletItem.cpp
    Dataflow/ControlOutletItem.cpp
    Dataflow/MidiInletItem.cpp
    Dataflow/MidiOutletItem.cpp
    Dataflow/AudioInletItem.cpp
    Dataflow/AudioOutletItem.cpp
    Dataflow/CableInspector.cpp
    Dataflow/DropPortInScenario.cpp
    score_plugin_dataflow.cpp
)


# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

# Link
target_link_libraries(${PROJECT_NAME} PRIVATE score_plugin_scenario score_plugin_audio score_plugin_protocols)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
