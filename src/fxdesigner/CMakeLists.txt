project(fx_designer)

qt5_add_resources(DESIGNER_RESOURCES_RCC "${CMAKE_CURRENT_SOURCE_DIR}/parser/parser.qrc")

add_executable(fx_designer
    designer.cpp
    widget.hpp
    widget.cpp
    view.hpp
    view.cpp
    inspector.hpp
    inspector.cpp
    handles.hpp
    ${DESIGNER_RESOURCES_RCC}
)
target_link_libraries(fx_designer PRIVATE
  score_lib_base
  score_lib_inspector
  score_lib_process
  score_plugin_inspector
)
