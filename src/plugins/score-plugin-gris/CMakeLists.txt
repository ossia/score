project(score_plugin_gris LANGUAGES CXX)

score_common_setup()

find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)

set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/GainMatrix.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/Mbap.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/Quaternion.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/SpeakerSetup.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/SpeakerSetupIO.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/Spatializer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/Types.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/Vbap.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Executor.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Metadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Model.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/SpeakerSetupInlet.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_gris.hpp"
)

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/Mbap.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/Quaternion.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/SpeakerSetup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/SpeakerSetupIO.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/Spatializer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Algo/Vbap.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Executor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Model.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/Serialization.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Gris/SpeakerSetupInlet.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_gris.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(${PROJECT_NAME}
  PUBLIC ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets
         score_lib_base score_lib_process score_lib_state
         score_plugin_engine score_plugin_dataflow
)

setup_score_plugin(${PROJECT_NAME})
