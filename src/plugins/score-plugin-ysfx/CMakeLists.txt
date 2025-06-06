project(score_plugin_ysfx LANGUAGES CXX)

if(NOT TARGET score_plugin_engine)
  return()
endif()

score_common_setup()

if(SCORE_FAST_DEV_BUILD)
  return()
endif()

# Packages
find_package(Freetype)

find_package(Ysfx)
if(NOT TARGET Ysfx::Ysfx)
  return()
endif()

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_ysfx.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/YSFX/Commands/EditScript.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/YSFX/Commands/ScriptMacro.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/YSFX/Commands/CommandFactory.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/YSFX/Executor/Component.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/YSFX/ProcessFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/YSFX/ProcessMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/YSFX/ProcessModel.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/YSFX/ProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/YSFX/ProcessModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/YSFX/Executor/Component.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/YSFX/Commands/CommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_ysfx.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# TODO make this automatic in some way, and extend to other components
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(
  ${PROJECT_NAME} PUBLIC
  score_lib_base score_plugin_engine score_plugin_media
  Ysfx::Ysfx
)

if(TARGET Freetype::Freetype)
  target_link_libraries(
    ${PROJECT_NAME} PUBLIC Freetype::Freetype
  )
endif()

if(SCORE_FHS_BUILD OR SCORE_YSFX_WITH_FONTCONFIG)
  target_link_libraries(${PROJECT_NAME} PUBLIC -lfontconfig)
endif()

setup_score_plugin(${PROJECT_NAME})
