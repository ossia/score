project(score_addon_skeleton LANGUAGES CXX)

# General initialization
score_common_setup()

# Source files
set(HDRS
    Skeleton/Executor.hpp
    Skeleton/Inspector.hpp
    Skeleton/LocalTree.hpp
    Skeleton/Metadata.hpp
    Skeleton/Presenter.hpp
    Skeleton/Process.hpp
    Skeleton/View.hpp
    Skeleton/Layer.hpp
    Skeleton/CommandFactory.hpp

    score_addon_skeleton.hpp
)
set(SRCS
    Skeleton/Executor.cpp
    Skeleton/Inspector.cpp
    Skeleton/LocalTree.cpp
    Skeleton/Presenter.cpp
    Skeleton/Process.cpp
    Skeleton/View.cpp

    score_addon_skeleton.cpp
)


# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_engine)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
