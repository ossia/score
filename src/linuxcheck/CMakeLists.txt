add_executable(linuxcheck
  x11.cpp
  wayland.cpp
  linuxcheck.cpp
  diagnostics.hpp
)

set_target_properties(linuxcheck PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN True
    POSITION_INDEPENDENT_CODE True
)

target_compile_features(linuxcheck
  PRIVATE
    cxx_std_23
)

target_include_directories(linuxcheck
  PRIVATE
    "${X11_X11_INCLUDE_PATH}"
)

target_link_libraries(linuxcheck
  PRIVATE
    $<COMPILE_ONLY:ossia>
    $<BUILD_INTERFACE:fmt::fmt>
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

if(LINUX)
  install(
    TARGETS linuxcheck
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()
