if(NOT WIN32)
return()
endif()

set(ISCORE_BIN_INSTALL_DIR "bin")

find_package(Jamoma)
if(${Jamoma_FOUND})
    copy_in_folder_jamoma(${ISCORE_BIN_INSTALL_DIR} "${JAMOMA_LIBS}" "${JAMOMA_PLUGINS}")
endif()

### TODO InstallRequiredSystemLibraries ###
 # Compiler Runtime DLLs
if (MSVC)
   # Visual Studio
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP true)
    include(InstallRequiredSystemLibraries)
    install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION ${ISCORE_BIN_INSTALL_DIR})
else()
   # MinGW
    get_filename_component(MINGW_DLL_DIR ${CMAKE_CXX_COMPILER} PATH)
    install(FILES
            "${MINGW_DLL_DIR}/libgcc_s_dw2-1.dll"
            "${MINGW_DLL_DIR}/libstdc++-6.dll"
            "${MINGW_DLL_DIR}/libwinpthread-1.dll"
            DESTINATION ${ISCORE_BIN_INSTALL_DIR})
endif()


# Qt Libraries
if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    set(DEBUG_CHAR "d")
    get_property(APIJamoma_dll TARGET APIJamoma PROPERTY LOCATION_DEBUG)
else()
    set(DEBUG_CHAR "")
    get_property(APIJamoma_dll TARGET APIJamoma PROPERTY LOCATION_RELEASE)
endif()

get_target_property(QtCore_LOCATION Qt5::Core LOCATION)
get_filename_component(QT_DLL_DIR ${QtCore_LOCATION} PATH)
file(GLOB ICU_DLLS "${QT_DLL_DIR}/icu*.dll")

# TODO instead register them somewhere like the plug-ins.



install(FILES
  "${APIJamoma_dll}"
  ${ICU_DLLS}
  "${QT_DLL_DIR}/Qt5Core${DEBUG_CHAR}.dll"
  "${QT_DLL_DIR}/Qt5Gui${DEBUG_CHAR}.dll"
  "${QT_DLL_DIR}/Qt5Widgets${DEBUG_CHAR}.dll"
  "${QT_DLL_DIR}/Qt5Network${DEBUG_CHAR}.dll"
  "${QT_DLL_DIR}/Qt5Xml${DEBUG_CHAR}.dll"
  "${QT_DLL_DIR}/Qt5Svg${DEBUG_CHAR}.dll"
  "${QT_DLL_DIR}/Qt5Qml${DEBUG_CHAR}.dll"
  "${QT_DLL_DIR}/Qt5OpenGL${DEBUG_CHAR}.dll"
  "${QT_DLL_DIR}/Qt5WebSockets${DEBUG_CHAR}.dll"
#  "${QT_DLL_DIR}/Qt5Test${DEBUG_CHAR}.dll"
  "${QT_DLL_DIR}/Qt5Quick${DEBUG_CHAR}.dll"
  "${QT_DLL_DIR}/Qt5QuickWidgets${DEBUG_CHAR}.dll"
  DESTINATION ${ISCORE_BIN_INSTALL_DIR})

# Qt conf file
install(
  FILES
    "${ISCORE_ROOT_SOURCE_DIR}/CMake/Deployment/Windows/qt.conf"
    "${ISCORE_ROOT_SOURCE_DIR}/base/lib/resources/i-score.ico"
  DESTINATION
    ${ISCORE_BIN_INSTALL_DIR})

# Qt Platform Plugin
install(FILES
  "${QT_DLL_DIR}/../plugins/platforms/qwindows${DEBUG_CHAR}.dll"
  DESTINATION ${ISCORE_BIN_INSTALL_DIR}/plugins/platforms)


# NSIS metadata

set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_EXECUTABLES "i-score.exe;i-score")

set(CPACK_COMPONENTS_ALL i-score)

set(CPACK_MONOLITHIC_INSTALL TRUE)
set(CPACK_NSIS_PACKAGE_NAME "i-score")
set(CPACK_PACKAGE_ICON "${ISCORE_ROOT_SOURCE_DIR}\\\\base\\\\lib\\\\resources\\\\i-score.ico")
set(CPACK_NSIS_MUI_ICON "${CPACK_PACKAGE_ICON}")
set(CPACK_NSIS_MUI_UNIICON "${CPACK_PACKAGE_ICON}")

set(CPACK_NSIS_HELP_LINK "http:\\\\\\\\www.i-score.org")
set(CPACK_NSIS_URL_INFO_ABOUT "http:\\\\\\\\www.i-score.org")
set(CPACK_NSIS_CONTACT "i-score-devs@lists.sourceforge.net")

set(CPACK_NSIS_COMPRESSOR "/SOLID lzma")

set(CPACK_NSIS_MENU_LINKS
    "bin/i-score.exe" "i-score"
    "http://www.i-score.org" "i-score website"
    )


set(CPACK_NSIS_DEFINES "!include ${CMAKE_CURRENT_LIST_DIR}\\\\Deployment\\\\Windows\\\\FileAssociation.nsh")
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
\\\${registerExtension} '\\\$INSTDIR\\\\bin\\\\i-score.exe' '.scorejson' 'i-score score'

SetOutPath '\\\$INSTDIR\\\\bin'
CreateShortcut '\\\$DESKTOP\\\\i-score.lnk' '\\\$INSTDIR\\\\bin\\\\i-score.exe' '' '\\\$INSTDIR\\\\bin\\\\i-score.ico'
SetRegView 64
WriteRegStr HKEY_LOCAL_MACHINE 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\i-score.exe' '' '$INSTDIR\\\\bin\\\\i-score.exe'
WriteRegStr HKEY_LOCAL_MACHINE 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\i-score.exe' 'Path' '$INSTDIR\\\\bin\\\\;$INSTDIR\\\\bin\\\\plugins\\\\;'
")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
Delete '$DESKTOP\\\\i-score.lnk'
DeleteRegKey HKLM 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\i-score.exe'
\\\${unregisterExtension} '.scorejson' 'i-score score'
")

