project(score_plugin_spline3d LANGUAGES CXX)

score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline3D/Model.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline3D/GeneratorDialog.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline3D/Metadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline3D/Execution.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline3D/Commands.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline3D/Widget.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_spline3d.hpp"
)

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline3D/Model.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline3D/Widget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline3D/Execution.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Spline3D/Commands.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_spline3d.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(${PROJECT_NAME}
  PUBLIC ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets
            score_lib_base score_lib_device
            score_plugin_deviceexplorer score_lib_process
            score_lib_state score_lib_inspector score_plugin_scenario
            score_plugin_gfx
)

setup_score_plugin(${PROJECT_NAME})
