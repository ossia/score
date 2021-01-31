project(score_plugin_vst LANGUAGES CXX)

score_common_setup()

if(NOT TARGET score_plugin_media)
  return()
endif()

# Packages
find_package(${QT_VERSION} OPTIONAL_COMPONENTS WebSockets)
if(NOT TARGET ${QT_PREFIX}::WebSockets)
  # VST support requires websockets
  return()
endif()

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/ApplicationPlugin.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Loader.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/EffectModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Widgets.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Executor.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Control.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Node.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Library.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/vst-compat.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Commands.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Settings.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_vst.hpp"
  )
set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/ApplicationPlugin.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Loader.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Control.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/EffectModel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Widgets.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Executor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Commands.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Vst/Settings.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_vst.cpp"
  )

if(APPLE)
  set(HDRS ${HDRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/Vst/AppleUI/vstwindow.h"
    )
  set(SRCS ${SRCS}
    "${CMAKE_CURRENT_SOURCE_DIR}/Vst/AppleUI/vstwindow.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vst/AppleUI/vst2x.mm"
    )

  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/Vst/AppleUI/vstwindow.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vst/AppleUI/vst2x.mm"
    PROPERTIES
      SKIP_UNITY_BUILD_INCLUSION 1
  )
else()
  set(SRCS ${SRCS}
    "${CMAKE_CURRENT_SOURCE_DIR}/Vst/LinuxUI/vst.cpp"
    )
endif()

add_library(${PROJECT_NAME}
  ${SRCS} ${HDRS}
)

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                     ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets ${QT_PREFIX}::WebSockets
                     ${CMAKE_DL_LIBS}
                     score_lib_base score_plugin_automation score_plugin_media
)

if(APPLE)
  find_library(AppKit_FK AppKit)
  find_library(Foundation_FK Foundation)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    ${Foundation_FK}
    ${AppKit_FK}
    )
endif()

setup_score_plugin(${PROJECT_NAME})
