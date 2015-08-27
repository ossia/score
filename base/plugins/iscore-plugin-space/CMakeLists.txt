cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_space)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)

find_package(GiNaC)
if(NOT GINAC_FOUND)
    return()
endif()

# Files & main target
include_directories("${SCENARIO_PROCESS_INTERFACE_FOLDER}")
include_directories("${DEVICEEXPLORER_INTERFACE_FOLDER}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")

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
"${CMAKE_CURRENT_SOURCE_DIR}/src/Space/SpacePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Space/SpaceView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/AreaParser.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/AreaModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/AreaPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/AreaFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/AreaFactoryList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/SingletonAreaFactoryList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/AreaView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Generic/GenericAreaFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Generic/GenericAreaModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Generic/GenericAreaPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Generic/GenericAreaView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Circle/CircleAreaFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Circle/CircleAreaModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Circle/CircleAreaPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/Area/Circle/CircleAreaView.cpp"

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
"${CMAKE_CURRENT_SOURCE_DIR}/src/Commands/AddCircle.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/src/SpaceControl.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/src/iscore_plugin_space.cpp"
)

#~ if(TARGET iscore_lib_inspector)
    #~ set(INSPECTOR_SRCS
    #~
    #~ )
#~
    #~ set(SRCS ${SRCS} ${INSPECTOR_SRCS})
#~
    #~ file(GLOB_RECURSE INSPECTOR_HDRS
        #~ "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/*.hpp")
#~
    #~ set(HDRS ${HDRS} ${INSPECTOR_HDRS})
#~ endif()

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                     Qt5::Core Qt5::Widgets
                     iscore_lib_base iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state)

target_link_libraries(${PROJECT_NAME} cln ginac)
#~ if(TARGET iscore_lib_inspector)
    #~ # TODO instead put the definition in the INTERFACE of iscore_lib_inspector
    #~ target_link_libraries(${PROJECT_NAME} iscore_lib_inspector)
#~ endif()

iscore_cotire(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/
        COMPONENT DynamicRuntime)


add_subdirectory(tests)
