project(score_plugin_ysfx LANGUAGES CXX)

if(NOT TARGET score_plugin_engine)
  return()
endif()

score_common_setup()

# Packages
find_library(YSFX_LIBRARY NAMES ysfx)
find_path(YSFX_HEADERS ysfx.h)

if(NOT YSFX_LIBRARY)
  return()
endif()

if(NOT YSFX_HEADERS)
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

target_include_directories(${PROJECT_NAME} PUBLIC
  ${YSFX_HEADERS}
)
target_link_libraries(${PROJECT_NAME} PUBLIC
                     score_lib_base score_plugin_engine
                     ${YSFX_LIBRARY} fontconfig freetype)

setup_score_plugin(${PROJECT_NAME})
