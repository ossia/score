project(score_plugin_packagemanager LANGUAGES CXX)

# General initialization
score_common_setup()

# Source files
set(HDRS
  PackageManager/Factory.hpp
  PackageManager/Model.hpp
  PackageManager/Presenter.hpp
  PackageManager/View.hpp
  PackageManager/PluginItemModel.hpp
  PackageManager/FileDownloader.hpp

  score_plugin_packagemanager.hpp
)
set(SRCS
  PackageManager/Model.cpp
  PackageManager/Presenter.cpp
  PackageManager/View.cpp
  PackageManager/PluginItemModel.cpp
  PackageManager/FileDownloader.cpp

  score_plugin_packagemanager.cpp
)


# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_library)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})


