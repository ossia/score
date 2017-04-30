cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_mapping LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_mapping.hpp"
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

)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Commands/ChangeAddresses.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Commands/MappingCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingColors.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Inspector/MappingInspectorWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_mapping.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                     Qt5::Core Qt5::Widgets
                     iscore_lib_base iscore_lib_device iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state iscore_plugin_curve)

setup_iscore_plugin(${PROJECT_NAME})
