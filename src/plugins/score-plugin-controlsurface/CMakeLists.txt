project(score_plugin_controlsurface LANGUAGES CXX)

# General initialization
score_common_setup()

# Source files
set(HDRS
    ControlSurface/Executor.hpp
    ControlSurface/LocalTree.hpp
    ControlSurface/Metadata.hpp
    ControlSurface/Presenter.hpp
    ControlSurface/Process.hpp
    ControlSurface/View.hpp
    ControlSurface/Layer.hpp
    ControlSurface/CommandFactory.hpp
    ControlSurface/Commands.hpp

    score_plugin_controlsurface.hpp
)
set(SRCS
    ControlSurface/Executor.cpp
    ControlSurface/LocalTree.cpp
    ControlSurface/Presenter.cpp
    ControlSurface/Process.cpp
    ControlSurface/View.cpp

    score_plugin_controlsurface.cpp
)


# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_engine)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
