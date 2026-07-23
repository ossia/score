add_library(Gist STATIC
  "${CMAKE_CURRENT_LIST_DIR}/Gist/src/CoreFrequencyDomainFeatures.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Gist/src/CoreTimeDomainFeatures.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Gist/src/Gist.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Gist/src/MFCC.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Gist/src/OnsetDetectionFunction.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Gist/src/WindowFunctions.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Gist/src/Yin.cpp"
)
target_compile_definitions(Gist PUBLIC USE_OSSIA_FFT=1)
target_include_directories(Gist SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}/Gist/src")
target_link_libraries(Gist PUBLIC ossia)
set_target_properties(Gist PROPERTIES UNITY_BUILD 0)
