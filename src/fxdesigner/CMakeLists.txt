project(fx_designer)
add_executable(fx_designer designer.cpp)
target_link_libraries(fx_designer PRIVATE
  score_lib_base
  score_lib_inspector
  score_plugin_inspector
)
