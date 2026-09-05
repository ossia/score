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
        "${cxx_path}/libwinpthread-1.dll"
#        ${MINGW64_LIB}/../bin/zlib1.dll
  )

  # DirectX shader compiler runtime, shipped exactly like libc++ / libunwind
  # above: appended to CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS so it flows through the
  # same install(FILES) below, into the same directory as score.exe.
  #
  # WHY IT HAS TO SHIP. Qt's D3D12 backend runtime-compiles HLSL, and for shader
  # model 6.0 and above it goes through DXC, which it loads by name at runtime
  # (qrhid3d12.cpp, compileHlslShaderSource). The SDK's Qt is built WITH that
  # support, so without the library deployed score's d3d12ShaderVersion() finds
  # no dxcompiler and falls back, silently, to shader model 5.0. At SM 5.0 there
  # is no SV_ViewID, so MULTIVIEW shaders cannot compile on D3D12 at all, and
  # wave intrinsics are unavailable. With these two DLLs reachable, D3D12
  # reaches shader model 6.x and native multiview works (up to
  # D3D12_MAX_VIEW_INSTANCE_COUNT == 4 views).
  #
  # TWO libraries, not one. dxcompiler.dll is the compiler; dxil.dll is the
  # signing library, and without it DXC emits UNSIGNED DXIL which D3D12 refuses
  # to load unless Developer Mode is on -- i.e. it works for us and fails for
  # every customer. Warn loudly rather than ship half of it.
  #
  # ossia/sdk 86207a70 (MSYS/dxc.sh) installs both into <sdk>/bin, which is two
  # levels above the compiler's own directory.
  get_filename_component(_score_sdk_root "${cxx_path}/../.." ABSOLUTE)
  foreach(_score_dxc_dll dxcompiler.dll dxil.dll)
    if(EXISTS "${_score_sdk_root}/bin/${_score_dxc_dll}")
      list(APPEND CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS
           "${_score_sdk_root}/bin/${_score_dxc_dll}")
    else()
      message(WARNING
        "${_score_dxc_dll} not found in ${_score_sdk_root}/bin -- it will not be "
        "shipped. Qt's D3D12 backend loads it at runtime for shader model 6.x; "
        "without it score silently falls back to SM 5.0, losing multiview and "
        "wave intrinsics. Update the ossia SDK (see MSYS/dxc.sh).")
    endif()
  endforeach()
endif()

include(InstallRequiredSystemLibraries)
install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
        DESTINATION ${SCORE_BIN_INSTALL_DIR}
        COMPONENT OssiaScore)

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
