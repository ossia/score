cmake_minimum_required(VERSION 3.0)
project(iscore_lib_dummyprocess)
iscore_common_setup()

find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets ) # QuickWidgets)

set(DUMMYPROCESS_HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/DummyModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyLayerModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyLayerPresenter.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyLayerView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyState.hpp"
)
set(DUMMYPROCESS_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/DummyModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyLayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyLayerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyLayerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyLayerPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyLayerModelSerialization.cpp"
)

if(TARGET iscore_lib_inspector)
    set(DUMMYPROCESS_HDRS ${DUMMYPROCESS_HDRS}
            "${CMAKE_CURRENT_SOURCE_DIR}/DummyInspectorWidget.hpp")
endif()


# qt5_add_resources(DUMMYPROCESS_QRCS "${CMAKE_CURRENT_SOURCE_DIR}/resources/DummyResources.qrc")

add_library(iscore_lib_dummyprocess ${DUMMYPROCESS_HDRS} ${DUMMYPROCESS_SRCS}  ${DUMMYPROCESS_QRCS})
target_link_libraries(iscore_lib_dummyprocess
    Qt5::Core Qt5::Widgets # Qt5::QuickWidgets
    iscore_lib_base iscore_lib_state iscore_lib_process)

if(TARGET iscore_lib_inspector)
    target_link_libraries(iscore_lib_dummyprocess iscore_lib_inspector)
endif()

setup_iscore_library(iscore_lib_dummyprocess)

set_property(TARGET iscore_lib_dummyprocess APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
