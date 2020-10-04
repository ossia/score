project(score_plugin_nodal LANGUAGES CXX)

# General initialization
score_common_setup()

# Source files
set(HDRS
    Nodal/Executor.hpp
    Nodal/LocalTree.hpp
    Nodal/Metadata.hpp
    Nodal/Presenter.hpp
    Nodal/Process.hpp
    Nodal/View.hpp
    Nodal/Layer.hpp
    Nodal/CommandFactory.hpp
    Nodal/Commands.hpp

    score_plugin_nodal.hpp
)
set(SRCS
    Nodal/Executor.cpp
    Nodal/LocalTree.cpp
    Nodal/Presenter.cpp
    Nodal/Process.cpp
    Nodal/View.cpp
    Nodal/Commands.cpp

    score_plugin_nodal.cpp
)


# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_scenario score_plugin_engine)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
