if(OSSIA_SDK AND APPLE)
  set(Suil_FOUND FALSE)
  return()
endif()

find_path(
    Suil_INCLUDE_DIR suil/suil.h
    HINTS
        "${OSSIA_SDK}/lv2/include/"
    PATH_SUFFIXES
        suil-0
    )

set(Suil_NAMES suil suil-0)
find_library(Suil_LIBRARY
  NAMES
    suil
    suil-0
  HINTS
    "${OSSIA_SDK}/lv2/lib64"
)

if(Suil_INCLUDE_DIR AND Suil_LIBRARY)
  set(Suil_FOUND TRUE)
endif()
if(UNIX AND NOT APPLE)
  get_filename_component(Suil_FOLDER "${Suil_LIBRARY}" DIRECTORY)
  if(NOT ((EXISTS "${Suil_FOLDER}/libsuil_x11_in_qt6.so") OR (EXISTS "${Suil_FOLDER}/suil-0/libsuil_x11_in_qt6.so")))
    unset(Suil_INCLUDE_DIR)
    unset(Suil_LIBRARY)
    unset(Suil_FOUND)
    include("${3RDPARTY_FOLDER}/suil.cmake")
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Suil DEFAULT_MSG
    Suil_LIBRARY Suil_INCLUDE_DIR)

if(Suil_FOUND AND NOT TARGET Suil::Suil)
    set(Suil_LIBRARIES ${Suil_LIBRARY})
    set(Suil_INCLUDE_DIRS ${Suil_INCLUDE_DIR})

    string(REGEX MATCH "(dll|so|dylib)$" IS_SHARED "${Suil_LIBRARY}")

    if(IS_SHARED)
      add_library(Suil SHARED IMPORTED GLOBAL)
      set_target_properties(Suil PROPERTIES
          INTERFACE_COMPILE_DEFINITIONS SUIL_SHARED)
    else()
      add_library(Suil STATIC IMPORTED GLOBAL)
    endif()

    set_target_properties(Suil PROPERTIES
        IMPORTED_LOCATION ${Suil_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${Suil_INCLUDE_DIR})
else()
    set(Suil_LIBRARIES)
    set(Suil_INCLUDE_DIRS)
endif()
