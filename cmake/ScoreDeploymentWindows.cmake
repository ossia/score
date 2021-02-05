if(NOT WIN32)
  return()
endif()

set(SCORE_BIN_INSTALL_DIR ".")

# Compiler Runtime DLLs
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON)

if(MINGW)
#  get_target_property(ZLIB_LOCATION ZLIB::ZLIB IMPORTED_LOCATION_RELEASE)
#  if(NOT ZLIB_LOCATION)
#    get_target_property(ZLIB_LOCATION ZLIB::ZLIB IMPORTED_LOCATION_DEBUG)
#    if(NOT ZLIB_LOCATION)
#      get_target_property(ZLIB_LOCATION ZLIB::ZLIB IMPORTED_LOCATION)
#    endif()
#  endif()
#
#  get_filename_component(MINGW64_LIB ${ZLIB_LOCATION} DIRECTORY)

  get_filename_component(cxx_path ${CMAKE_CXX_COMPILER} PATH)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS
        ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
        ${cxx_path}/libc++.dll
        ${cxx_path}/libunwind.dll
#        ${MINGW64_LIB}/../bin/zlib1.dll
#        ${MINGW64_LIB}/../bin/libwinpthread-1.dll
  )
endif()

include(InstallRequiredSystemLibraries)
install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
        DESTINATION ${SCORE_BIN_INSTALL_DIR}
        COMPONENT OssiaScore)

# Qt Libraries
set(DEBUG_CHAR "$<$<CONFIG:Debug>:d>")

get_target_property(QtCore_LOCATION ${QT_PREFIX}::Core LOCATION)
get_filename_component(QT_DLL_DIR ${QtCore_LOCATION} PATH)

if(NOT OSSIA_STATIC)
  install(FILES "$<TARGET_FILE:ossia>"
          DESTINATION ${SCORE_BIN_INSTALL_DIR})
endif()

# Qt conf file
install(
  FILES
    "${SCORE_ROOT_SOURCE_DIR}/cmake/Deployment/Windows/qt.conf"
    "${SCORE_ROOT_SOURCE_DIR}/src/lib/resources/score.ico"
  DESTINATION
    ${SCORE_BIN_INSTALL_DIR}
  COMPONENT
    OssiaScore
)


if(EXISTS "${QT_DLL_DIR}/Qt5Core${DEBUG_CHAR}.dll")
  install(FILES
    "${QT_DLL_DIR}/Qt5Core${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt5Gui${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt5Widgets${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt5Network${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt5Xml${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt5Svg${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt5Qml${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt5OpenGL${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt5WebSockets${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt5SerialPort${DEBUG_CHAR}.dll"
    DESTINATION "${SCORE_BIN_INSTALL_DIR}")

  # Qt plug-ins
  set(QT_PLUGINS_DIR "${QT_DLL_DIR}/../plugins")
  set(QT_QML_PLUGINS_DIR "${QT_DLL_DIR}/../qml")
  set(plugin_dest_dir "${SCORE_BIN_INSTALL_DIR}/plugins")

  install(FILES "${QT_PLUGINS_DIR}/platforms/qwindows${DEBUG_CHAR}.dll" DESTINATION "${plugin_dest_dir}/platforms")
  install(FILES "${QT_PLUGINS_DIR}/imageformats/qsvg${DEBUG_CHAR}.dll" DESTINATION "${plugin_dest_dir}/imageformats")
  install(FILES "${QT_PLUGINS_DIR}/iconengines/qsvgicon${DEBUG_CHAR}.dll" DESTINATION "${plugin_dest_dir}/iconengines")
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
set(CPACK_INSTALL_STRIP ON)
set(CPACK_STRIP_FILES ON)
set(CMAKE_INSTALL_DO_STRIP ON)

set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_EXECUTABLES "score.exe;ossia score")

set(CPACK_COMPONENTS_ALL OssiaScore)
set(CPACK_MONOLITHIC_INSTALL FALSE)

set(CPACK_NSIS_PACKAGE_NAME "ossia score")
set(CPACK_PACKAGE_ICON "${SCORE_ROOT_SOURCE_DIR}\\\\src\\\\lib\\\\resources\\\\score.ico")
set(CPACK_NSIS_MUI_ICON "${CPACK_PACKAGE_ICON}")
set(CPACK_NSIS_MUI_UNIICON "${CPACK_PACKAGE_ICON}")

set(CPACK_NSIS_HELP_LINK "https:\\\\\\\\ossia.io")
set(CPACK_NSIS_URL_INFO_ABOUT "https:\\\\\\\\ossia.io")
set(CPACK_NSIS_CONTACT "https:\\\\\\\\gitter.im\\\\OSSIA\\\\score")

set(CPACK_NSIS_COMPRESSOR "/SOLID lzma")

set(CPACK_NSIS_MENU_LINKS
    "score.exe" "ossia score"
    "https://ossia.io" "ossia website"
    )


set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
    "!include ${CMAKE_CURRENT_LIST_DIR}\\\\Deployment\\\\Windows\\\\FileAssociation.nsh"
)

set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
\\\${registerExtension} '\\\$INSTDIR\\\\score.exe' '.scorejson' 'score file'
\\\${registerExtension} '\\\$INSTDIR\\\\score.exe' '.score' 'score file'

SetOutPath '\\\$INSTDIR'
CreateShortcut '\\\$DESKTOP\\\\score.lnk' '\\\$INSTDIR\\\\score.exe' '' '\\\$INSTDIR\\\\score.ico'
SetRegView 64
WriteRegStr HKEY_LOCAL_MACHINE 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\score.exe' '' '$INSTDIR\\\\score.exe'
WriteRegStr HKEY_LOCAL_MACHINE 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\score.exe' 'Path' '$INSTDIR;$INSTDIR\\\\plugins\\\\;'
")

set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
Delete '$DESKTOP\\\\score.lnk'
DeleteRegKey HKLM 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\score.exe'
\\\${unregisterExtension} '.scorejson' 'score score'
\\\${unregisterExtension} '.score' 'score score'
")

