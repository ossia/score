cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(score_plugin_analysis LANGUAGES CXX)

add_library(
  score_plugin_analysis
  Analysis/Centroid.hpp
  Analysis/ComplexSpectralDifference.hpp
  Analysis/Crest.hpp
  Analysis/EnergyDifference.hpp
  Analysis/Envelope.hpp
  Analysis/Flatness.hpp
  Analysis/GistState.hpp
  Analysis/HighFrequencyContent.hpp
  Analysis/Kurtosis.hpp
  Analysis/MFCC.hpp
  Analysis/Pitch.hpp
  Analysis/Rolloff.hpp
  Analysis/SpectralDifference.hpp
  Analysis/SpectralDifference_HWR.hpp
  Analysis/ZeroCrossing.hpp

  score_plugin_analysis.hpp
  score_plugin_analysis.cpp

  "${3RDPARTY_FOLDER}/Gist/src/CoreFrequencyDomainFeatures.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/CoreFrequencyDomainFeatures.h"
  "${3RDPARTY_FOLDER}/Gist/src/CoreTimeDomainFeatures.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/CoreTimeDomainFeatures.h"
  "${3RDPARTY_FOLDER}/Gist/src/Gist.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/Gist.h"
  "${3RDPARTY_FOLDER}/Gist/src/MFCC.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/MFCC.h"
  "${3RDPARTY_FOLDER}/Gist/src/OnsetDetectionFunction.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/OnsetDetectionFunction.h"
  "${3RDPARTY_FOLDER}/Gist/src/WindowFunctions.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/WindowFunctions.h"
  "${3RDPARTY_FOLDER}/Gist/src/Yin.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/Yin.h"
)

target_compile_definitions(score_plugin_analysis PRIVATE USE_OSSIA_FFT=1)
target_include_directories(score_plugin_analysis PRIVATE "${3RDPARTY_FOLDER}/Gist/src")
target_link_libraries(score_plugin_analysis PUBLIC score_plugin_engine)
if(TARGET kfr)
  target_link_libraries(score_plugin_analysis PUBLIC "$<BUILD_INTERFACE:kfr>")
endif()
setup_score_plugin(score_plugin_analysis)


