project(score_plugin_vst3 LANGUAGES CXX)

# General initialization
score_common_setup()

if(NOT TARGET score_plugin_media)
  return()
endif()

if(MSVC)
  message("VST 3 support does not work yet with MSVC.")
  return()
endif()

if(WIN32)
  # Needed because on windows we need admin permissions which does not work on CI
  # (see smtg_create_directory_as_admin_win)
  set(SMTG_PLUGIN_TARGET_PATH "${CMAKE_CURRENT_BINARY_DIR}/vst3_path" CACHE PATH "vst3 folder")
  file(MAKE_DIRECTORY "${SMTG_PLUGIN_TARGET_PATH}")
endif()
set(VST3_SDK_ROOT "${3RDPARTY_FOLDER}/vst3")

add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3")
# Source files
set(HDRS
  Vst3/ApplicationPlugin.hpp
  Vst3/Commands.hpp
  Vst3/Control.hpp
  Vst3/EffectModel.hpp
  Vst3/Executor.hpp
  Vst3/Node.hpp
  Vst3/Plugin.hpp
  Vst3/Library.hpp
  Vst3/Widgets.hpp
  Vst3/UI/Window.hpp
  Vst3/UI/PlugFrame.hpp
  Vst3/UI/Linux/PlugFrame.hpp

  score_plugin_vst3.hpp
)
set(SRCS
  Vst3/ApplicationPlugin.cpp
  Vst3/Commands.cpp
  Vst3/Control.cpp
  Vst3/EffectModel.cpp
  Vst3/Executor.cpp
  Vst3/Plugin.cpp
  Vst3/Widgets.cpp

  score_plugin_vst3.cpp
)


# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

if(WIN32)
  target_sources(${PROJECT_NAME} PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
    Vst3/UI/Window.cpp
  )
elseif(APPLE)
  target_sources(${PROJECT_NAME} PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm"
    Vst3/UI/macOS/Window.mm
  )

  set_source_files_properties(
      "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm"
      Vst3/UI/macOS/Window.mm
    PROPERTIES
      SKIP_UNITY_BUILD_INCLUSION 1
  )
else()
  target_sources(${PROJECT_NAME} PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_linux.cpp"
    Vst3/UI/Window.cpp
  )
endif()

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

# Link
target_link_libraries(${PROJECT_NAME}
  PRIVATE
    score_plugin_engine
    score_plugin_dataflow
    score_plugin_media
    sdk_common sdk_hosting
)

if(APPLE)
  find_library(AppKit_FK AppKit)
  find_library(Foundation_FK Foundation)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    ${Foundation_FK}
    ${AppKit_FK}
    )
endif()

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
