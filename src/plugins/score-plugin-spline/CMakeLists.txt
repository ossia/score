project(score_plugin_spline LANGUAGES CXX)

score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/Model.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/Presenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/GeneratorDialog.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/View.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/Metadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/Execution.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/Commands.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_spline.hpp"
)

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/Model.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/Presenter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/View.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/Execution.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline/Commands.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_spline.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(${PROJECT_NAME}
  PUBLIC ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets
            score_lib_base score_lib_device
            score_plugin_deviceexplorer score_lib_process
            score_lib_state score_lib_inspector score_plugin_scenario
)

setup_score_plugin(${PROJECT_NAME})
