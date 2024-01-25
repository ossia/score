function(disable_qt_plugins theTarget)
  if(NOT TARGET ${QT_PREFIX}::Core)
    return()
  endif()

  get_target_property(QtType ${QT_PREFIX}::Core TYPE)

  if(NOT ("${QtType}" STREQUAL "STATIC_LIBRARY"))
    return()
  endif()

  set_target_properties(${theTarget}
    PROPERTIES
      QT_NO_PLUGINS
     "${Qt5Core_PLUGINS};${Qt5Gui_PLUGINS};${Qt5Widgets_PLUGINS};${Qt5Network_PLUGINS};${Qt5Qml_PLUGINS}"
  )
endfunction()

function(enable_minimal_qt_plugins theTarget gui)
  if(NOT TARGET ${QT_PREFIX}::Core)
    return()
  endif()

  get_target_property(QtType ${QT_PREFIX}::Core TYPE)
  if(NOT ("${QtType}" STREQUAL "STATIC_LIBRARY"))
    return()
  endif()

  function(link_if_exists _theLib)
    if(TARGET ${_theLib})
      target_link_libraries(${theTarget} PRIVATE ${_theLib})
    endif()
  endfunction()

  link_if_exists(${QT_PREFIX}::QOffscreenIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QMinimalIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWindowsIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QCocoaIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QTlsBackendOpenSSLPlugin)
  if(${gui})
  link_if_exists(${QT_PREFIX}::QXcbIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QXcbGlxIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QXcbEglIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandEglPlatformIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandQtShellIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandWlShellIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandFullScreenShellV1IntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandEglClientBufferPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandXdgShellIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandXdgShellV5IntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandXdgShellV6IntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandWlShellIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandBradientDecorationPlugin)
  link_if_exists(${QT_PREFIX}::QWaylandFullScreenShellV1IntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QEglFSIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QEglFSX11IntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QEglFSKmsEglDeviceIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QEglFSKmsGbmIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QVncIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QWasmIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QMinimalEglIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QVkKhrIntegrationPlugin)
  link_if_exists(${QT_PREFIX}::QEvdevKeyboardPlugin)
  link_if_exists(${QT_PREFIX}::QEvdevMousePlugin)
  link_if_exists(${QT_PREFIX}::QEvdevTabletPlugin)
  link_if_exists(${QT_PREFIX}::QEvdevTouchScreenPlugin)
  link_if_exists(${QT_PREFIX}::QXdgDesktopPortalThemePlugin)

  link_if_exists(${QT_PREFIX}::QJpegPlugin)
  link_if_exists(${QT_PREFIX}::QTiffPlugin)
  link_if_exists(${QT_PREFIX}::QGifPlugin)
  link_if_exists(${QT_PREFIX}::QTgaPlugin)
  link_if_exists(${QT_PREFIX}::QWebpPlugin)

  link_if_exists(Qt6::qtqmlcoreplugin)
  link_if_exists(Qt6::qmlplugin)
  link_if_exists(Qt6::qmlmetaplugin)
  link_if_exists(Qt6::qmlshapesplugin)
  link_if_exists(Qt6::quick2plugin)
  link_if_exists(Qt6::particlesplugin)
  link_if_exists(Qt6::qtquickcontrols2plugin)
  endif()
endfunction()