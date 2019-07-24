project(fx_designer)
add_executable(fx_designer
    designer.cpp
    widget.hpp
    widget.cpp
    view.hpp
    view.cpp
    inspector.hpp
    inspector.cpp
    handles.hpp
)
target_link_libraries(fx_designer PRIVATE
  score_lib_base
  score_lib_inspector
  score_lib_process
  score_plugin_inspector
)
