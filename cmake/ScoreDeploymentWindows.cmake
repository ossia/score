if(NOT WIN32)
  return()
endif()

set(DEBUG_CHAR "$<$<CONFIG:Debug>:d>")


# Compiler Runtime DLLs
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON)
if(NOT SCORE_FHS_BUILD)
  # Copy DLLs if needed
  include(ScoreDeploymentWindows.dlls)

  # Qt conf file
  install(
    FILES
      "${SCORE_ROOT_SOURCE_DIR}/src/lib/resources/score.ico"
    DESTINATION
      "${SCORE_BIN_INSTALL_DIR}"
    COMPONENT
      OssiaScore
  )

  # Ship a snapshot of the user library inside the installer. It is staged in
  # $INSTDIR/default-library and the NSIS installer copies it into the user's
  # library folder on install (see ScoreDeploymentWindows.nsis), so the app
  # finds it on first start and skips the network download.
  set(SCORE_USER_LIBRARY_DIR "${CMAKE_BINARY_DIR}/score-user-library")
  if(NOT EXISTS "${SCORE_USER_LIBRARY_DIR}/score-user-library-master/package.json")
    set(SCORE_USER_LIBRARY_ZIP "${CMAKE_BINARY_DIR}/score-user-library.zip")
    message(STATUS "Downloading score user library snapshot for the installer...")
    file(DOWNLOAD
      "https://github.com/ossia/score-user-library/archive/refs/heads/master.zip"
      "${SCORE_USER_LIBRARY_ZIP}"
      STATUS SCORE_USER_LIBRARY_DL_STATUS
    )
    list(GET SCORE_USER_LIBRARY_DL_STATUS 0 SCORE_USER_LIBRARY_DL_CODE)
    if(SCORE_USER_LIBRARY_DL_CODE EQUAL 0)
      file(ARCHIVE_EXTRACT
        INPUT "${SCORE_USER_LIBRARY_ZIP}"
        DESTINATION "${SCORE_USER_LIBRARY_DIR}"
      )
    else()
      message(WARNING "Could not download score user library: ${SCORE_USER_LIBRARY_DL_STATUS}")
    endif()
  endif()

  if(EXISTS "${SCORE_USER_LIBRARY_DIR}/score-user-library-master/package.json")
    install(
      DIRECTORY
        "${SCORE_USER_LIBRARY_DIR}/score-user-library-master/"
      DESTINATION
        "${SCORE_BIN_INSTALL_DIR}/default-library"
      COMPONENT
        OssiaScore
      PATTERN ".git" EXCLUDE
      PATTERN ".github" EXCLUDE
    )
  endif()

  # Faust stuff
  if(EXISTS "${CMAKE_BINARY_DIR}/src/plugins/score-plugin-faust/faustlibs-prefix/src/faustlibs")
    install(
      DIRECTORY
        "${CMAKE_BINARY_DIR}/src/plugins/score-plugin-faust/faustlibs-prefix/src/faustlibs/"
      DESTINATION
        "${SCORE_BIN_INSTALL_DIR}/faust"
      COMPONENT
          OssiaScore
        PATTERN ".git" EXCLUDE
        PATTERN "doc" EXCLUDE
        PATTERN "*.html" EXCLUDE
        PATTERN "*.svg" EXCLUDE
        PATTERN "*.scad" EXCLUDE
        PATTERN "*.obj" EXCLUDE
        PATTERN "build" EXCLUDE
        PATTERN ".gitignore" EXCLUDE
        PATTERN "modalmodels" EXCLUDE
     )
  endif()
endif()


install(CODE "
    file(GLOB_RECURSE DLLS_TO_REMOVE \"*.dll\")
    list(FILTER DLLS_TO_REMOVE INCLUDE REGEX \"qml/.*/*dll\")
    if(NOT \"\${DLLS_TO_REMOVE}\" STREQUAL \"\")
      file(REMOVE \${DLLS_TO_REMOVE})
    endif()

    file(GLOB_RECURSE PDB_TO_REMOVE \"*.pdb\")
    if(NOT \"\${PDB_TO_REMOVE}\" STREQUAL \"\")
      file(REMOVE \${PDB_TO_REMOVE})
    endif()

    file(REMOVE_RECURSE
        \"\${CMAKE_INSTALL_PREFIX}/lib\"
    )
")

if(NOT TARGET score_plugin_jit)
    install(CODE "
        file(REMOVE_RECURSE
            \"\${CMAKE_INSTALL_PREFIX}/include\"
            \"\${CMAKE_INSTALL_PREFIX}/lib\"
            \"\${CMAKE_INSTALL_PREFIX}/Ossia\"
        )
    ")
endif()

# NSIS metadata
include(ScoreDeploymentWindows.nsis)