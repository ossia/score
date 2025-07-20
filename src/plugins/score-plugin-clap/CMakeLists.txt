project(score_plugin_clap LANGUAGES CXX)

score_common_setup()

# Files & main target
set(HDRS
  Clap/ApplicationPlugin.hpp
  Clap/EffectModel.hpp
  Clap/Executor.hpp
  Clap/Library.hpp
  
  score_plugin_clap.hpp
)

set(SRCS
  Clap/ApplicationPlugin.cpp
  Clap/EffectModel.cpp
  Clap/Executor.cpp
  Clap/Library.cpp
  
  score_plugin_clap.cpp
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_link_libraries(${PROJECT_NAME} PUBLIC
  score_lib_base 
  score_lib_process
  score_plugin_engine
  score_plugin_library
  
  # Link to CLAP headers
  ${CMAKE_SOURCE_DIR}/3rdparty/clap
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/3rdparty/clap/include
)

# Platform-specific libraries for dynamic loading
if(WIN32)
  # No additional libraries needed on Windows
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE 
    "-framework CoreFoundation"
  )
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

setup_score_plugin(${PROJECT_NAME})