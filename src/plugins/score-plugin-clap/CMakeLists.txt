project(score_plugin_clap LANGUAGES CXX)

score_common_setup()

# Files & main target
set(HDRS
  Clap/ApplicationPlugin.hpp
  Clap/EffectModel.hpp
  Clap/Executor.hpp
  Clap/Library.hpp
  Clap/Window.hpp

  score_plugin_clap.hpp
)

set(SRCS
  Clap/ApplicationPlugin.cpp
  Clap/EffectModel.cpp
  Clap/Executor.cpp
  Clap/Library.cpp
  Clap/Window.cpp

  score_plugin_clap.cpp
)

# Platform-specific window implementations
if(0)
  set(SRCS ${SRCS}
    Clap/AppleUI/ClapWindow.mm
  )

  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/Clap/AppleUI/ClapWindow.mm"
    PROPERTIES
      SKIP_UNITY_BUILD_INCLUSION 1
  )
else()
  set(SRCS ${SRCS}
    Clap/LinuxUI/ClapWindow.cpp
  )
endif()

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(${PROJECT_NAME} PUBLIC
  score_lib_base
  score_lib_process
  score_plugin_engine
  score_plugin_library
  score_plugin_media
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/3rdparty/clap/include
)

# Platform-specific libraries for dynamic loading
if(WIN32)
  # No additional libraries needed on Windows
elseif(APPLE)
  find_library(AppKit_FK AppKit)
  find_library(Foundation_FK Foundation)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    "-framework CoreFoundation"
    ${Foundation_FK}
    ${AppKit_FK}
  )
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

setup_score_plugin(${PROJECT_NAME})
