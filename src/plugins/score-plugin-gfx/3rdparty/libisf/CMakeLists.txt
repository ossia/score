cmake_minimum_required(VERSION 3.17)

project(libisf CXX)
include(GenerateStaticExport)
option(ISF_ENABLE_EDITOR "Enable ISF editor" OFF)
option(ISF_ENABLE_TESTS "Enable ISF tests" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

add_library(isf
    src/sajson.h
    src/isf.hpp
    src/isf.cpp
)
target_include_directories(isf
  PUBLIC
    src/
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

generate_export_header(isf BASE_NAME ISF)

if(ISF_ENABLE_TESTS)
  add_executable(tst_glsl450 tests/glsl450)
  add_test(tst_glsl450 tst_glsl450)
  target_link_libraries(tst_glsl450 PRIVATE isf)
endif()

if(ISF_ENABLE_EDITOR)
  find_package(Qt5 REQUIRED COMPONENTS Quick QuickWidgets Multimedia Xml DBus)

  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)
  add_executable(editor
      editor/main.cpp
      editor/audio_reader.hpp
      editor/camera_reader.hpp
      editor/code_editor.hpp
      editor/control_factory.hpp
      editor/image_surface.hpp
      editor/material.hpp
      editor/shader.hpp
      editor/shader_item.hpp
      editor/shader_node.hpp
      editor/video_reader.hpp
      editor/window.hpp
      editor/resources.qrc
  )

  find_package(KF5TextEditor NO_MODULE)
  if(${KF5TextEditor_FOUND})
      find_package(KF5Parts NO_MODULE)
      target_link_libraries(editor PRIVATE KF5::Parts KF5::TextEditor)
      target_compile_definitions(editor PRIVATE KTEXTEDITOR)
  endif()
  target_link_libraries(editor PRIVATE isf Qt5::Gui Qt5::Quick Qt5::QuickWidgets Qt5::Multimedia )
  target_compile_options(editor PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)
endif()
