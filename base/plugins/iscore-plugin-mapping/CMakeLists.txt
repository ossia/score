cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_mapping)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets PrintSupport Xml Test)

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
target_link_libraries(${PROJECT_NAME}
                     Qt5::Core Qt5::Widgets Qt5::PrintSupport
                     iscore_lib_base iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state iscore_plugin_curve)

if(TARGET iscore_lib_inspector)
    target_link_libraries(${PROJECT_NAME} iscore_lib_inspector)
endif()

setup_iscore_plugin(${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
