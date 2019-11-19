project(score_plugin_ui LANGUAGES CXX)

add_library(
  score_plugin_ui
  "${CMAKE_CURRENT_SOURCE_DIR}/Ui/SignalDisplay.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_ui.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_ui.cpp"
)

target_link_libraries(score_plugin_ui PUBLIC score_plugin_engine)

setup_score_plugin(score_plugin_ui)
