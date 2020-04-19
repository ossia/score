project(vstpuppet CXX)
find_package(Qt5 5.9 OPTIONAL_COMPONENTS WebSockets)

if(NOT TARGET Qt5::WebSockets)
  return()
endif()
add_executable(ossia-score-vstpuppet vstpuppet.cpp "${SCORE_SRC}/plugins/score-plugin-media/Media/Effect/VST/VSTLoader.cpp")
target_compile_definitions(ossia-score-vstpuppet PUBLIC HAS_VST2)
target_link_libraries(
  ossia-score-vstpuppet
  PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::WebSockets
    ${CMAKE_DL_LIBS})

if(APPLE)
    find_library(Foundation_FK Foundation)
    target_link_libraries(ossia-score-vstpuppet PRIVATE
        ${Foundation_FK}
        )
endif()

target_include_directories(
  ossia-score-vstpuppet
  PRIVATE
    "${SCORE_SRC}/plugins/score-plugin-media")

if(APPLE AND DEPLOYMENT_BUILD)
  set_target_properties(
      ossia-score-vstpuppet
      PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY score.app/Contents/MacOS)
  install(
    TARGETS ossia-score-vstpuppet
    RUNTIME DESTINATION score.app/Contents/MacOS
    COMPONENT OssiaScore)
elseif(WIN32)
  install(
    TARGETS ossia-score-vstpuppet
    RUNTIME DESTINATION .
    COMPONENT OssiaScore)
else()
  install(
    TARGETS ossia-score-vstpuppet
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()

if(SCORE_COTIRE)
  cotire(ossia-score-vstpuppet)
endif()
