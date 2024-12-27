if(OSSIA_USE_SYSTEM_LIBRARIES)
    return()
endif()

if(NOT LV2_PATH)
  return()
endif()

find_package(X11)
if(NOT X11_FOUND)
  return()
endif()

set(Suil_INCLUDE_DIR "${3RDPARTY_FOLDER}/suil/include")

add_library(Suil SHARED
    "${3RDPARTY_FOLDER}/suil/src/host.c"
    "${3RDPARTY_FOLDER}/suil/src/instance.c"
)
add_library(Suil::Suil ALIAS Suil)
set_target_properties(Suil PROPERTIES
    LIBRARY_OUTPUT_NAME "suil-0"
    VERSION 0
    SOVERSION 0
)

target_compile_definitions(Suil PRIVATE "SUIL_MODULE_DIR=\"${CMAKE_BINARY_DIR}/lib/suil-0\"")

target_include_directories(Suil
    PUBLIC
      "${Suil_INCLUDE_DIR}"
    PRIVATE
      "${3RDPARTY_FOLDER}/suil/src"
      "${LV2_PATH}"
)

add_library(suil_x11_in_qt6 MODULE
    "${3RDPARTY_FOLDER}/suil/src/x11.c"
    "${3RDPARTY_FOLDER}/suil/src/x11_in_qt.cpp"
)
set_target_properties(suil_x11_in_qt6 PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/suil-0"
)

target_include_directories(suil_x11_in_qt6
    PRIVATE "${3RDPARTY_FOLDER}/suil/src"
)

target_link_libraries(suil_x11_in_qt6
    PRIVATE Suil Qt6::Core Qt6::Gui Qt6::Widgets X11::X11
)

set(Suil_LIBRARY Suil)
set(Suil_FOUND 1 )
