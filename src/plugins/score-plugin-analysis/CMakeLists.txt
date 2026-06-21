cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(score_plugin_analysis LANGUAGES CXX)

if(SCORE_FAST_DEV_BUILD)
  return()
endif()

if(NOT TARGET score_plugin_avnd)
  message("WARNING: score_plugin_avnd not found, skipping ${PROJECT_NAME}")
  return()
endif()

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
)

add_library(Gist STATIC
  "${3RDPARTY_FOLDER}/Gist/src/CoreFrequencyDomainFeatures.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/CoreTimeDomainFeatures.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/Gist.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/MFCC.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/OnsetDetectionFunction.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/WindowFunctions.cpp"
  "${3RDPARTY_FOLDER}/Gist/src/Yin.cpp"
)
target_compile_definitions(Gist PUBLIC USE_OSSIA_FFT=1)
target_include_directories(Gist SYSTEM PUBLIC "${3RDPARTY_FOLDER}/Gist/src")
target_link_libraries(Gist PUBLIC ossia)
set_target_properties(Gist PROPERTIES UNITY_BUILD 0)

target_link_libraries(score_plugin_analysis PUBLIC score_plugin_engine score_plugin_avnd Gist)
if(OSSIA_ENABLE_KFR AND TARGET kfr)
  link_whole_archive(score_plugin_analysis PRIVATE kfr)
  link_whole_archive(score_plugin_analysis PRIVATE kfr_dsp)
  list(APPEND SCORE_FEATURES_LIST analysis_kfr)
endif()
setup_score_plugin(score_plugin_analysis)


