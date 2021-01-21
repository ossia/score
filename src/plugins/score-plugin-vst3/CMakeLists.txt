project(score_plugin_vst3 LANGUAGES CXX)

# General initialization
score_common_setup()

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
  )
elseif(APPLE)
  target_sources(${PROJECT_NAME} PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm"
  )
else()
  target_sources(${PROJECT_NAME} PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_linux.cpp"
  )
endif()

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

# Link
target_link_libraries(${PROJECT_NAME}
  PRIVATE
    score_plugin_engine
    score_plugin_dataflow
    sdk_common sdk_hosting
)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
