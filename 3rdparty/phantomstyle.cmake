add_library(phantomstyle STATIC
  "${CMAKE_CURRENT_LIST_DIR}/phantomstyle/src/phantom/phantomcolor.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/phantomstyle/src/phantom/phantomstyle.cpp"
)
target_include_directories(phantomstyle SYSTEM PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/phantomstyle/src")
target_link_libraries(phantomstyle PUBLIC
  ${QT_PREFIX}::Core ${QT_PREFIX}::Gui ${QT_PREFIX}::Widgets)
set_target_properties(phantomstyle PROPERTIES UNITY_BUILD 0)
