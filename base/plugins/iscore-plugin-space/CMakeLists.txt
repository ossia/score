cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_space LANGUAGES CXX)

iscore_common_setup()

find_package(GiNaC)
if(NOT GINAC_FOUND)
    return()
endif()

if(NOT TARGET iscore_plugin_ossia)
  return()
endif()

find_package(VTK)
if(NOT ${VTK_FOUND})
    return()
endif()
include(${VTK_USE_FILE})

file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/src/SpaceProcessFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/SpaceProcess.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/SpaceLayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/SpaceLayerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/SpaceLayerView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/SpaceProcessPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/SpaceProcessProxyLayerModel.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/src/Computation/ComputationModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Space/DimensionModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Space/ViewportModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Space/SpaceModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/AreaParser.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/AreaModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/AreaPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/AreaFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/SingletonAreaFactoryList.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Generic/GenericAreaFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Generic/GenericAreaModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Generic/GenericAreaPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Generic/GenericAreaView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Circle/CircleAreaFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Circle/CircleAreaModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Circle/CircleAreaPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Circle/CircleAreaView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Pointer/PointerAreaFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Pointer/PointerAreaModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Pointer/PointerAreaPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Pointer/PointerAreaView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/ComputationsTab.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/SpaceTab.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/AreaTab.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/AreaWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/AreaSelectionWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/ParameterWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/ViewportEditWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/DimensionEditWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/SpaceWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/SpaceGuiWindow.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Commands/AddArea.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Commands/SpaceCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Executor/ProcessExecutor.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/LocalTree/ProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/LocalTree/ProcessComponentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/LocalTree/AreaComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/LocalTree/ComputationComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/LocalTree/AreaComponentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/LocalTree/ComputationComponentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/LocalTree/GenericAreaComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/LocalTree/GenericComputationComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/LocalTree/GenericAreaComponentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/LocalTree/GenericComputationComponentFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/SpaceApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/iscore_plugin_space.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(${PROJECT_NAME} PRIVATE
                     Qt5::Core Qt5::Widgets
                     iscore_lib_base iscore_lib_device iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state iscore_plugin_ossia ${VTK_LIBRARIES}
           ${GINAC_LIBRARIES})

setup_iscore_plugin(${PROJECT_NAME})
#setup_iscore_tests(tests)
