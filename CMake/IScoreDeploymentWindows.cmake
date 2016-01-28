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
else()
    set(DEBUG_CHAR "")
endif()

get_target_property(QtCore_LOCATION Qt5::Core LOCATION)
get_filename_component(QT_DLL_DIR ${QtCore_LOCATION} PATH)
file(GLOB ICU_DLLS "${QT_DLL_DIR}/icu*.dll")

# TODO instead register them somewhere like the plug-ins.
install(FILES
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
  "${QT_DLL_DIR}/Qt5Test${DEBUG_CHAR}.dll"
  DESTINATION ${ISCORE_BIN_INSTALL_DIR})

# Qt conf file
install(
  FILES
    "${CMAKE_CURRENT_LIST_DIR}/Deployment/Windows/qt.conf"
  DESTINATION
    ${ISCORE_BIN_INSTALL_DIR})

# Qt Platform Plugin
install(FILES
  "${QT_DLL_DIR}/../plugins/platforms/qwindows${DEBUG_CHAR}.dll"
  DESTINATION ${ISCORE_BIN_INSTALL_DIR}/plugins/platforms)
