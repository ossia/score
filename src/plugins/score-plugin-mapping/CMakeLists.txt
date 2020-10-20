project(score_plugin_mapping LANGUAGES CXX)

score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_mapping.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Commands/ChangeAddresses.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Commands/MappingCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Commands/MinMaxCommands.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Inspector/MappingInspectorFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Inspector/MappingInspectorWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingColors.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingPresenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingProcessMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingExecution.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/LocalTree.hpp"

)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingExecution.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Commands/ChangeAddresses.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Commands/MappingCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingColors.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Inspector/MappingInspectorWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_mapping.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC score_lib_base score_lib_inspector score_lib_process score_plugin_curve)

setup_score_plugin(${PROJECT_NAME})
