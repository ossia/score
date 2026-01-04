find_path(
  YSFX_S_INCLUDE_DIR ysfx-s.h
)
find_library(YSFX_S_LIBRARY NAMES ysfx-s)

find_path(
  YSFX_INCLUDE_DIR ysfx.h
)
find_library(YSFX_LIBRARY NAMES ysfx)


if(YSFX_S_INCLUDE_DIR AND YSFX_S_LIBRARY)
  set(YSFX_FOUND TRUE)
  set(YSFX_LIBRARY ${YSFX_S_LIBRARY})
  set(YSFX_INCLUDE_DIR ${YSFX_S_INCLUDE_DIR})
elseif(YSFX_INCLUDE_DIR AND YSFX_LIBRARY)
  set(YSFX_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Ysfx DEFAULT_MSG
    YSFX_LIBRARY YSFX_INCLUDE_DIR)

if(YSFX_FOUND)
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
