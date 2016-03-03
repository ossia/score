cmake_minimum_required(VERSION 3.0)
project(iscore_lib_dummyprocess LANGUAGES CXX)
iscore_common_setup()

find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)# Qml QuickWidgets)

set(DUMMYPROCESS_HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyLayerModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyLayerPresenter.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyLayerView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyState.hpp"
)
set(DUMMYPROCESS_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyLayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyLayerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyLayerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyLayerPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyLayerModelSerialization.cpp"
)

if(TARGET iscore_lib_inspector)
    set(DUMMYPROCESS_HDRS ${DUMMYPROCESS_HDRS}
            "${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/DummyInspectorWidget.hpp")
endif()


qt5_add_resources(DUMMYPROCESS_QRCS "${CMAKE_CURRENT_SOURCE_DIR}/DummyProcess/resources/DummyResources.qrc")

add_library(iscore_lib_dummyprocess ${DUMMYPROCESS_HDRS} ${DUMMYPROCESS_SRCS}  ${DUMMYPROCESS_QRCS})
target_link_libraries(iscore_lib_dummyprocess PUBLIC
    Qt5::Core Qt5::Widgets # Qt5::Qml Qt5::QuickWidgets
    iscore_lib_base iscore_lib_state iscore_lib_process)

if(TARGET iscore_lib_inspector)
  target_link_libraries(iscore_lib_dummyprocess PUBLIC iscore_lib_inspector)
endif()

setup_iscore_library(iscore_lib_dummyprocess)

set_property(TARGET iscore_lib_dummyprocess APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
