project(score_plugin_audio LANGUAGES CXX)

score_common_setup()

# Files & main target
set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Model.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Presenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/View.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Factory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/JackInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/SDLInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/PortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/ALSAPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/ASIOPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/CoreAudioPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/GenericPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/MMEPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/WASAPIPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/WDMKSPortAudioInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/DummyInterface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioApplicationPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioPreviewExecutor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioTick.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_audio.hpp"
    )
set(SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Model.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/View.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/ALSAPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/GenericPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioPreviewExecutor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioTick.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_audio.cpp"
)


add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

if(OSSIA_PROTOCOL_AUDIO)
    target_sources(${PROJECT_NAME} PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioDevice.hpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioDevice.cpp"
    )
endif()

target_link_libraries(${PROJECT_NAME}
        PUBLIC
          ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets
          score_lib_base score_lib_device score_lib_state score_plugin_scenario score_plugin_deviceexplorer
          ossia
)

setup_score_plugin(${PROJECT_NAME})
