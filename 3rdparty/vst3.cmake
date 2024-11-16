if(OSSIA_USE_SYSTEM_LIBRARIES AND LINUX)
  find_path(VST3_SDK_MODULE_DIR
      public.sdk/source/vst/hosting/module_linux.cpp
      PATH_SUFFIXES vst3sdk
  )
  if(VST3_SDK_MODULE_DIR)
    find_library(VST3_SDK_COMMON_LIBRARY
        NAMES sdk_common
        PATH_SUFFIXES vst3sdk
    )
    find_library(VST3_SDK_HOSTING_LIBRARY
        NAMES sdk_hosting
        PATH_SUFFIXES vst3sdk
    )
    find_path(VST3_SDK_INCLUDE_DIR
        pluginterfaces/base/funknown.h
        PATH_SUFFIXES vst3sdk
        PATHS "${VST3_SDK_MODULE_DIR}"
    )
    if(VST3_SDK_COMMON_LIBRARY AND VST3_SDK_HOSTING_LIBRARY AND VST3_SDK_INCLUDE_DIR AND VST3_SDK_MODULE_DIR)
        add_library(sdk_common INTERFACE IMPORTED GLOBAL)
        target_link_libraries(sdk_common INTERFACE "${VST3_SDK_COMMON_LIBRARY}")
        target_include_directories(sdk_common INTERFACE "$<BUILD_INTERFACE:${VST3_SDK_INCLUDE_DIR}>")

        add_library(sdk_hosting INTERFACE IMPORTED GLOBAL)
        target_link_libraries(sdk_hosting INTERFACE  "${VST3_SDK_HOSTING_LIBRARY}")
        set(VST3_SDK_ROOT "${VST3_SDK_MODULE_DIR}")
        return()
    endif()
  endif()
endif()

if(NOT TARGET sdk_common)
  if(WIN32)
    # Needed because on windows we need admin permissions which does not work on CI
    # (see smtg_create_directory_as_admin_win)
    set(SMTG_PLUGIN_TARGET_PATH "${CMAKE_CURRENT_BINARY_DIR}/vst3_path" CACHE PATH "vst3 folder")
    file(MAKE_DIRECTORY "${SMTG_PLUGIN_TARGET_PATH}")
  endif()
  set(VST3_SDK_ROOT "${3RDPARTY_FOLDER}/vst3")
  set(SMTG_ADD_VST3_HOSTING_SAMPLES 0)
  set(SMTG_ADD_VST3_HOSTING_SAMPLES 0 CACHE INTERNAL "")

  # VST3 uses COM APIs which require no virtual dtors in interfaces
  if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-non-virtual-dtor")
  endif()
  block()
    if(MSVC)
      add_compile_options("/w")
    else()
      add_compile_options("-w")
    endif()
    add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3" SYSTEM)
  endblock()
  target_compile_options(sdk_common PRIVATE "${OSSIA_DISABLE_WARNINGS_FLAGS}")
  target_compile_options(sdk_hosting PRIVATE "${OSSIA_DISABLE_WARNINGS_FLAGS}")
endif()
