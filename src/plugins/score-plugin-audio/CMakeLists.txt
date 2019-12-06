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
    "${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioPanel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_audio.hpp"
    )
set(SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Model.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/Presenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/Settings/View.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/ALSAPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/ASIOPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/CoreAudioPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/GenericPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/MMEPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/WASAPIPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/WDMKSPortAudioInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioApplicationPlugin.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Audio/AudioPanel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_audio.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${MAPPER_SRCS})

target_link_libraries(${PROJECT_NAME}
        PUBLIC
          Qt5::Core Qt5::Widgets
          score_lib_base score_lib_device 
          ossia
)

setup_score_plugin(${PROJECT_NAME})
