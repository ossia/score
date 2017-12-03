
find_path(VST2_INCLUDE_DIR aeffectx.h
    PATHS
      "${3RDPARTY_FOLDER}/VST3 SDK/pluginterfaces/vst2.x"
)

if(VST2_INCLUDE_DIR)
  add_library(Vst2SDK INTERFACE)
  target_include_directories(Vst2SDK INTERFACE ${VST2_INCLUDE_DIR})
endif()
# Handle the QUIETLY and REQUIRED arguments and set SNDFILE_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vst2SDK DEFAULT_MSG
    VST2_INCLUDE_DIR)
mark_as_advanced(Vst2SDK_INCLUDE_DIR)
