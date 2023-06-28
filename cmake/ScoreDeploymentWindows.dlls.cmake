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
        "${cxx_path}/libc++.dll"
        "${cxx_path}/libunwind.dll"
#        ${MINGW64_LIB}/../bin/zlib1.dll
  )
endif()

if(TARGET score_plugin_pd)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS
    ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
    "${3RDPARTY_FOLDER}/libpd/libs/mingw64/libwinpthread-1.dll"
  )
endif()

include(InstallRequiredSystemLibraries)
install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
        DESTINATION ${SCORE_BIN_INSTALL_DIR}
        COMPONENT OssiaScore)
endif()

# Qt Libraries
get_target_property(QtCore_LOCATION ${QT_PREFIX}::Core LOCATION)
get_filename_component(QT_DLL_DIR ${QtCore_LOCATION} PATH)

if(NOT OSSIA_STATIC)
  install(FILES "$<TARGET_FILE:ossia>"
          DESTINATION ${SCORE_BIN_INSTALL_DIR})
endif()


if(EXISTS "${QT_DLL_DIR}/Qt6Core${DEBUG_CHAR}.dll")
  install(FILES
    "${QT_DLL_DIR}/Qt6Core${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6Gui${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6Widgets${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6Network${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6Xml${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6Svg${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6Qml${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6OpenGL${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6WebSockets${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6SerialPort${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6StateMachine${DEBUG_CHAR}.dll"
    "${QT_DLL_DIR}/Qt6ShaderTools${DEBUG_CHAR}.dll"
    DESTINATION "${SCORE_BIN_INSTALL_DIR}")

  # Qt plug-ins
  set(QT_PLUGINS_DIR "${QT_DLL_DIR}/../plugins")
  set(QT_QML_PLUGINS_DIR "${QT_DLL_DIR}/../qml")
  set(plugin_dest_dir "${SCORE_BIN_INSTALL_DIR}/plugins")

  install(FILES "${QT_PLUGINS_DIR}/platforms/qwindows${DEBUG_CHAR}.dll" DESTINATION "${plugin_dest_dir}/platforms")
  install(FILES "${QT_PLUGINS_DIR}/imageformats/qsvg${DEBUG_CHAR}.dll" DESTINATION "${plugin_dest_dir}/imageformats")
  install(FILES "${QT_PLUGINS_DIR}/iconengines/qsvgicon${DEBUG_CHAR}.dll" DESTINATION "${plugin_dest_dir}/iconengines")
endif()