find_path(
    Lilv_INCLUDE_DIR lilv/lilv.h
    HINTS
        /usr/include/lilv-0
    )

set(Lilv_NAMES lilv lilv-0)
find_library(Lilv_LIBRARY NAMES ${Lilv_NAMES})

if(Lilv_INCLUDE_DIR AND Lilv_LIBRARY)
  set(Lilv_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Lilv DEFAULT_MSG
    Lilv_LIBRARY Lilv_INCLUDE_DIR)

if(Lilv_FOUND)
    set(Lilv_LIBRARIES ${Lilv_LIBRARY})
    set(Lilv_INCLUDE_DIRS ${Lilv_INCLUDE_DIR})

    string(REGEX MATCH "(a|lib)$" IS_STATIC "${Lilv_LIBRARY}")
    if(IS_STATIC)
        add_library(Lilv STATIC IMPORTED GLOBAL)
    else()
        add_library(Lilv SHARED IMPORTED GLOBAL)
        set_target_properties(Lilv PROPERTIES
            INTERFACE_COMPILE_DEFINITIONS LILV_SHARED)
    endif()
    set_target_properties(Lilv PROPERTIES
        IMPORTED_LOCATION ${Lilv_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${Lilv_INCLUDE_DIR})
else()
    set(Lilv_LIBRARIES)
    set(Lilv_INCLUDE_DIRS)
endif()
