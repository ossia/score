cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_autom3d LANGUAGES CXX)

iscore_common_setup()

if(NOT TARGET iscore_plugin_ossia)
    return()
endif()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

find_package(VTK COMPONENTS
  vtkCommonCore
  vtkFiltersSources
  vtkGUISupportQt
  vtkInteractionWidgets
)
if(NOT ${VTK_FOUND})
    return()
endif()
include(${VTK_USE_FILE})

# Files & main target
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/*.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Autom3DSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Autom3DLayerSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Autom3DView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Autom3DModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Autom3DFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Autom3DLayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Autom3DPanelProxy.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Commands/InitAutom3D.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Commands/ChangeAddress.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Commands/Autom3DCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Executor/Executor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Executor/Component.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Executor/ComponentFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/State/Autom3DState.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Panel/AutomWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Panel/AutomWidget.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Panel/AutomWidget.ui"

"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Autom3DColors.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Autom3DProcessMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_autom3d.cpp"
)

if(TARGET iscore_lib_inspector)
    set(INSPECTOR_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Inspector/Autom3DInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Inspector/Autom3DStateInspector.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Inspector/Autom3DStateInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Inspector/Autom3DInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Inspector/CurvePointInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Inspector/CurvePointInspectorFactory.cpp"
    )

    set(SRCS ${SRCS} ${INSPECTOR_SRCS})

    file(GLOB_RECURSE INSPECTOR_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Inspector/*.hpp")

    set(HDRS ${HDRS} ${INSPECTOR_HDRS})
endif()



qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/Autom3D/Resources/Autom3DResources.qrc")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${QRCS})

iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(${PROJECT_NAME} PRIVATE
                     Qt5::Core Qt5::Widgets
                     iscore_lib_base iscore_lib_device iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state  iscore_plugin_ossia iscore_lib_dummyprocess
                     ${VTK_LIBRARIES} )

if(TARGET iscore_lib_inspector)
    target_link_libraries(${PROJECT_NAME} PRIVATE iscore_lib_inspector)
endif()

setup_iscore_plugin(${PROJECT_NAME})
