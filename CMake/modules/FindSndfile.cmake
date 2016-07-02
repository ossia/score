# - Find sndfile
# Find the native sndfile includes and libraries
#
#  SNDFILE_INCLUDE_DIR - where to find sndfile.h, etc.
#  SNDFILE_LIBRARIES   - List of libraries when using libsndfile.
#  SNDFILE_FOUND       - True if libsndfile found.

if(NO_LIBSNDFILE)
    #if(NOT SNDFILE_FIND_QUIETLY)
    #	message(STATUS "FindSndfile: sndfile deactivated (NO_LIBSNDFILE)")
    #endif()
    set(SNDFILE_FOUND False)
    set(SNDFILE_INCLUDE_DIR "nowhere")  # for onceonly check above
    set(SNDFILE_LIBRARIES "")
    add_definitions("-DNO_LIBSNDFILE")
elseif (SNDFILE_INCLUDE_DIR AND SNDFILE_LIBRARY)
    set(SNDFILE_LIBRARIES ${SNDFILE_LIBRARY})
    set(SNDFILE_FOUND TRUE)
else()
    set(ProgramFilesX86 "ProgramFiles(X86)")
    find_path(SNDFILE_INCLUDE_DIR sndfile.h
        PATHS
            "$ENV{ProgramFiles}/Mega-Nerd/libsndfile/include"
            "$ENV{${ProgramFilesX86}}/Mega-Nerd/libsndfile/include"
    )

    find_library(SNDFILE_LIBRARY NAMES sndfile sndfile-1 libsndfile libsndfile-1
        PATHS
            "$ENV{ProgramFiles}/Mega-Nerd/libsndfile/lib"
            "$ENV{${ProgramFilesX86}}/Mega-Nerd/libsndfile/lib"
    )

    # Handle the QUIETLY and REQUIRED arguments and set SNDFILE_FOUND to TRUE if
    # all listed variables are TRUE.
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Sndfile DEFAULT_MSG
        SNDFILE_LIBRARY SNDFILE_INCLUDE_DIR)

    if(SNDFILE_FOUND)
        set(SNDFILE_LIBRARIES ${SNDFILE_LIBRARY})
    else(SNDFILE_FOUND)
        set(SNDFILE_LIBRARIES)
    endif(SNDFILE_FOUND)

endif()
mark_as_advanced(SNDFILE_INCLUDE_DIR SNDFILE_LIBRARY)
