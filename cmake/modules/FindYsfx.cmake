find_path(
  YSFX_INCLUDE_DIR ysfx.h
)

set(YSFX_NAMES ysfx)
find_library(YSFX_LIBRARY NAMES ysfx)

if(YSFX_INCLUDE_DIR AND YSFX_LIBRARY)
  set(YSFX_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    YSFX DEFAULT_MSG
    YSFX_LIBRARY YSFX_INCLUDE_DIR)

if(YSFX_FOUND)
    set(YSFX_LIBRARIES ${YSFX_LIBRARY})
    set(YSFX_INCLUDE_DIRS ${YSFX_INCLUDE_DIR})

    string(REGEX MATCH "(dll|so|dylib)$" IS_SHARED "${YSFX_LIBRARY}")

    if(IS_SHARED)
      add_library(Ysfx::Ysfx SHARED IMPORTED GLOBAL)
      set_target_properties(Ysfx::Ysfx PROPERTIES
          INTERFACE_COMPILE_DEFINITIONS YSFX_SHARED)
    else()
      add_library(Ysfx::Ysfx STATIC IMPORTED GLOBAL)
    endif()

    set_target_properties(Ysfx::Ysfx PROPERTIES
        IMPORTED_LOCATION ${YSFX_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${YSFX_INCLUDE_DIR})
else()
    set(YSFX_LIBRARIES)
    set(YSFX_INCLUDE_DIRS)
endif()
