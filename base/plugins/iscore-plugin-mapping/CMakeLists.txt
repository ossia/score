cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_mapping LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/*.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingLayerSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingLayerModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Commands/ChangeAddresses.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Commands/MappingCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/MappingColors.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_mapping.cpp"
)

if(TARGET iscore_lib_inspector)
    set(INSPECTOR_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Inspector/MappingInspectorWidget.cpp"
    )

    set(SRCS ${SRCS} ${INSPECTOR_SRCS})

    file(GLOB_RECURSE INSPECTOR_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Inspector/*.hpp")

    set(HDRS ${HDRS} ${INSPECTOR_HDRS})
endif()

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/Mapping/Resources/MappingResources.qrc")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${QRCS})
iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                     Qt5::Core Qt5::Widgets
                     iscore_lib_base iscore_lib_device iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state iscore_plugin_curve)

if(TARGET iscore_lib_inspector)
  target_link_libraries(${PROJECT_NAME} PUBLIC iscore_lib_inspector)
endif()

setup_iscore_plugin(${PROJECT_NAME})
