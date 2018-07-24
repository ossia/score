find_path(
    Suil_INCLUDE_DIR suil/suil.h
    HINTS
        /usr/include/suil-0
        /usr/local/include/suil-0
    )

set(Suil_NAMES suil suil-0)
find_library(Suil_LIBRARY NAMES ${Suil_NAMES})

if(Suil_INCLUDE_DIR AND Suil_LIBRARY)
  set(Suil_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Suil DEFAULT_MSG
    Suil_LIBRARY Suil_INCLUDE_DIR)

if(Suil_FOUND)
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
