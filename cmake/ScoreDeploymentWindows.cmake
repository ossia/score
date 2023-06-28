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