project(score_plugin_lv2 LANGUAGES CXX)

include(ExternalProject)
score_common_setup()

if(NOT TARGET score_plugin_media)
  return()
endif()

if(SCORE_FAST_DEV_BUILD)
  return()
endif()

find_package(Lilv)
find_package(Suil)
find_path(LV2_PATH lv2/lv2plug.in/ns/ext/atom/atom.h
  HINTS
    "${OSSIA_SDK}/lv2/include"
)

if(NOT TARGET Lilv OR NOT TARGET Suil OR NOT LV2_PATH)
  return()
endif()


set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/ApplicationPlugin.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/EffectModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/lv2_atom_helpers.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/Context.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/Suil.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/Node.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/Window.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/Library.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_lv2.hpp"
)

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/ApplicationPlugin.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/EffectModel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/Context.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LV2/Window.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_lv2.cpp"
)


add_library(${PROJECT_NAME}
  ${SRCS} ${HDRS}
)

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                     ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets ${CMAKE_DL_LIBS}
                     score_plugin_media
)

# Lilv is used in API header, it must stay public
target_link_libraries(${PROJECT_NAME} PUBLIC Lilv)
target_include_directories(${PROJECT_NAME} 
  PRIVATE 
    3rdparty/readerwriterqueue
    ${LV2_PATH}
)

setup_score_plugin(${PROJECT_NAME})
