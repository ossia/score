project(score_plugin_pd LANGUAGES CXX)

if(MSVC)
  # On MSVC this needs winpthread. Since MSVC is just there for the occasional testing
  # and not for releases... "patches accepted"
  return()
endif()

set(PD_MULTI ON CACHE INTERNAL "")
set(CMAKE_C_VISIBILITY_PRESET "hidden")
add_subdirectory("${3RDPARTY_FOLDER}/libpd" "${CMAKE_BINARY_DIR}/libpd")
#set_target_properties(libpd PROPERTIES UNITY_BUILD 0)
set_target_properties(libpd_static PROPERTIES UNITY_BUILD 0)

if(NOT TARGET libpd_static)
  message("Problem occurring when building libpd")
  return()
endif()
score_common_setup()

# Packages
# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/PdProcess.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/PdLayer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/PdMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/PdLibrary.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/PdFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/Commands/PdCommandFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/Commands/EditPd.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/Executor/PdExecutor.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/Inspector/PdInspectorWidget.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_pd.hpp"
)

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/PdProcess.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/Commands/PdCommandFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/Executor/PdExecutor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Pd/Inspector/PdInspectorWidget.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_pd.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_include_directories(${PROJECT_NAME}
  PRIVATE
  "${3RDPARTY_FOLDER}/libpd/libpd_wrapper"
  "${3RDPARTY_FOLDER}/libpd/pure-data/src"
)
target_link_libraries(${PROJECT_NAME}
  PUBLIC
    score_plugin_engine
    score_plugin_scenario
    libpd_static)

setup_score_plugin(${PROJECT_NAME})


# setup_score_tests(Tests)
