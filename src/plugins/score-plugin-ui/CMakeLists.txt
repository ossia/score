project(score_plugin_ui LANGUAGES CXX)

if(NOT TARGET score_plugin_avnd)
  message("WARNING: score_plugin_avnd not found, skipping ${PROJECT_NAME}")
  return()
endif()

add_library(
  score_plugin_ui
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/AudioScope.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/AudioScope.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/BufferView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/BufferView.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/BufferDecode.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/BufferGather.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/BufferGatherPass.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/BufferGatherPass.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/ChannelLayout.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/LayerHelpers.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/Paint.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/Ring.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Loudness.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Loudness.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/MidiDisplay.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/SignalDisplay.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Spectrum.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Spectrum.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Tuner.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Tuner.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/SpectrumAnalysis.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/TextureLayers.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/TextureShapeMonitor.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/TextBox.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/ValueDisplay.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/VUMeter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/VUMeter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/PortProbe.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/PortProbe.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/TextureView.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/TextureView.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/VideoScope.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/VideoScope.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/Monitor/ImageAnalysis.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_ui.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_ui.cpp"
)

include("${3RDPARTY_FOLDER}/ebur128.cmake")
if(NOT TARGET Gist)
  include("${3RDPARTY_FOLDER}/gist.cmake")
endif()
target_link_libraries(score_plugin_ui PUBLIC score_plugin_avnd ebur128 Gist)
score_register_license(libebur128 URL "https://github.com/jiixyj/libebur128" FILES "${3RDPARTY_FOLDER}/libebur128/COPYING" NOTICES "${3RDPARTY_FOLDER}/libebur128/ebur128/queue/sys/queue.h")

setup_score_plugin(score_plugin_ui)
