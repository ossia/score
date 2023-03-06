find_path(
    Lilv_INCLUDE_DIR lilv/lilv.h
    HINTS
        "${OSSIA_SDK}/lv2/include/lilv-0"
        /usr/include/lilv-0
        /usr/local/include/lilv-0
    )

find_library(Lilv_LIBRARY 
  NAMES 
    lilv 
    lilv-0
  HINTS
    "${OSSIA_SDK}/lv2/lib64"
)

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

    string(REGEX MATCH "(dll|so|dylib)$" IS_SHARED "${Lilv_LIBRARY}")

    if(IS_SHARED)
      add_library(Lilv SHARED IMPORTED GLOBAL)
      set_target_properties(Lilv PROPERTIES
          INTERFACE_COMPILE_DEFINITIONS LILV_SHARED)
    else()
      find_library(Serd_LIBRARY NAMES serd-0 HINTS "${OSSIA_SDK}/lv2/lib64")
      find_library(Sord_LIBRARY NAMES sord-0 HINTS "${OSSIA_SDK}/lv2/lib64")
      find_library(Sratom_LIBRARY NAMES sratom-0 HINTS "${OSSIA_SDK}/lv2/lib64")
      find_library(Zix_LIBRARY NAMES zix-0 HINTS "${OSSIA_SDK}/lv2/lib64")
      add_library(Lilv STATIC IMPORTED GLOBAL)
      target_link_libraries(Lilv 
        INTERFACE 
          ${Serd_LIBRARY} ${Sord_LIBRARY} ${Sratom_LIBRARY} ${Suil_LIBRARY} ${Zix_LIBRARY}
      )
    endif()

    set_target_properties(Lilv PROPERTIES
        IMPORTED_LOCATION ${Lilv_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${Lilv_INCLUDE_DIR})
else()
    set(Lilv_LIBRARIES)
    set(Lilv_INCLUDE_DIRS)
endif()
