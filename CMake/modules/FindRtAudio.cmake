########################################################################
# Cmake module for finding RtAudio
#
# The following variabled will be defined:
#
#  RtAudio_FOUND
#  RtAudio_INCLUDE_DIR
#  RtAudio_LIBRARY
#


# ----------------------------------------------------------------------
# Find RtAudio include path
# ----------------------------------------------------------------------
find_path(RtAudio_INCLUDE_DIR RtAudio.h
  ${LibSourcey_DEPENDENCIES_SOURCE_DIR}/RtAudio
  /usr/local/include
  /usr/include
)


# ----------------------------------------------------------------------
# Find RtAudio library
# ----------------------------------------------------------------------
if(WIN32 AND MSVC)
  
  find_library(RtAudio_DEBUG_LIBRARY 
    NAMES 
      RtAudiod
    PATHS 
      ${LibSourcey_DEPENDENCIES_BUILD_DIR}/RtAudio
      ${LibSourcey_DEPENDENCIES_INSTALL_DIR}/lib
      /usr/lib 
      /usr/local/lib
    )
    
  find_library(RtAudio_RELEASE_LIBRARY 
    NAMES 
      RtAudio
    PATHS 
      ${LibSourcey_DEPENDENCIES_BUILD_DIR}/RtAudio
      ${LibSourcey_DEPENDENCIES_INSTALL_DIR}/lib
      /usr/lib 
      /usr/local/lib
    )
    
  if(RtAudio_DEBUG_LIBRARY OR RtAudio_RELEASE_LIBRARY)  
    if(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE)            
      if (RtAudio_RELEASE_LIBRARY) 
        list(APPEND RtAudio_LIBRARY "optimized" ${RtAudio_RELEASE_LIBRARY})
      endif()
      if (RtAudio_DEBUG_LIBRARY)
        list(APPEND RtAudio_LIBRARY "debug" ${RtAudio_DEBUG_LIBRARY})
      endif()
    else()    
      if (RtAudio_RELEASE_LIBRARY) 
        list(APPEND RtAudio_LIBRARY ${RtAudio_RELEASE_LIBRARY})
      elseif (RtAudio_DEBUG_LIBRARY) 
        list(APPEND RtAudio_LIBRARY ${RtAudio_DEBUG_LIBRARY})
      endif()
    endif()  
    mark_as_advanced(RtAudio_DEBUG_LIBRARY RtAudio_RELEASE_LIBRARY)
  endif()

else()

  # TODO: RtAudio lib names for various systems
  #set(RtAudio_LIB_NAMES "RtAudio rtaudio")

  find_library(RtAudio_LIBRARY 
    NAMES 
      rtaudio #${RtAudio_LIB_NAMES}
    PATHS 
      ${LibSourcey_DEPENDENCIES_BUILD_DIR}/RtAudio
      ${LibSourcey_DEPENDENCIES_INSTALL_DIR}/lib
      /usr/lib 
      /usr/local/lib
    )

endif()

if(RtAudio_LIBRARY AND RtAudio_INCLUDE_DIR)
  mark_as_advanced(RtAudio_LIBRARY RtAudio_INCLUDE_DIR)
  set(RtAudio_FOUND 1)
else()
  set(RtAudio_FOUND 0)
endif()

#message("RtAudio_LIBRARY=${RtAudio_LIBRARY}")
#message("RtAudio_INCLUDE_DIR=${RtAudio_INCLUDE_DIR}")
#message("LibSourcey_DEPENDENCIES_INSTALL_DIR=${LibSourcey_DEPENDENCIES_INSTALL_DIR}/lib")
#get_filename_component(RtAudio_LIBRARY_DIR "${RtAudio_LIBRARY}" PATH)
#get_filename_component(RtAudio_LIBRARY "${RtAudio_LIBRARY}" NAME)
  
      
# ----------------------------------------------------------------------
# Display status
# ----------------------------------------------------------------------
if(NOT RtAudio_FOUND)
   if(RtAudio_FIND_REQUIRED)
      message(FATAL_ERROR "RtAudio was not found. Please build dependencies first, or specify the path manually.")
   endif()
endif()
