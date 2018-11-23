if(SCORE_STATIC_QT)
    find_package(Qt5 5.3 REQUIRED COMPONENTS DBus Qml Quick)

    get_target_property(QtCore_LIB Qt5::Core LOCATION)
    get_filename_component(QT_LIB_FOLDER ${QtCore_LIB} DIRECTORY)

    find_library(qtharfbuzz_LIBRARY NAMES qtharfbuzz libqtharfbuzz HINTS ${QT_LIB_FOLDER})
    find_library(qtharfbuzzng_LIBRARY NAMES qtharfbuzzng libqtharfbuzzng HINTS ${QT_LIB_FOLDER})
    find_library(qtfreetype_LIBRARY NAMES qtfreetype libqtfreetype HINTS ${QT_LIB_FOLDER})
    find_library(Qt5XcbQpa_LIBRARY NAMES Qt5XcbQpa libQt5XcbQpa HINTS ${QT_LIB_FOLDER})
    find_library(qtpcre_LIBRARY NAMES qtpcre libqtpcre HINTS ${QT_LIB_FOLDER})
    find_library(qtpcre2_LIBRARY NAMES qtpcre2 libqtpcre2 HINTS ${QT_LIB_FOLDER})

    find_library(xcbxkb_LIBRARY NAMES xcb-xkb libxcb-xkb)

    find_library(qmldbg_local_LIBRARY NAMES qmldbg_local HINTS ${QT_LIB_FOLDER}/../plugins/qmltooling)
    find_library(qmldbg_tcp_LIBRARY NAMES qmldbg_tcp HINTS ${QT_LIB_FOLDER}/../plugins/qmltooling)

    find_library(Qt5PlatformSupport_LIBRARY NAMES Qt5PlatformSupport libQt5PlatformSupport HINTS ${QT_LIB_FOLDER})
    if(Qt5PlatformSupport_LIBRARY)
      add_library(Qt5PlatformSupport STATIC IMPORTED)
      set_target_properties(Qt5PlatformSupport PROPERTIES IMPORTED_LOCATION ${Qt5PlatformSupport_LIBRARY})
      set_property(TARGET Qt5PlatformSupport PROPERTY INTERFACE_LINK_LIBRARIES Qt5::Gui Qt5::DBus dbus-1 icudata m dl gthread-2.0 Xrender Xext X11 udev GL)
    endif()

    if(Qt5XcbQpa_LIBRARY)
      add_library(Qt5XcbQpa STATIC IMPORTED)
      set_target_properties(Qt5XcbQpa PROPERTIES IMPORTED_LOCATION ${Qt5XcbQpa_LIBRARY})

      if(TARGET Qt5PlatformSupport)
        set_property(TARGET Qt5XcbQpa PROPERTY INTERFACE_LINK_LIBRARIES Qt5PlatformSupport)
      endif()
    endif()
endif()

function(static_link_qt _target)
  if(SCORE_STATIC_QT)
  message("${Qt5Qml_PLUGINS}")
  target_link_libraries(
      ${_target} PRIVATE
      Qt5::Core Qt5::Gui Qt5::Widgets Qt5::DBus Qt5::Network Qt5::Svg Qt5::Xml Qt5::Qml ${Qt5Qml_PLUGINS} ${Qt5Qml_PLUGINS} Qt5::Quick
  )

  if(TARGET Qt5PlatformSupport)
    target_link_libraries(${_target} PRIVATE Qt5PlatformSupport)
  endif()
  if(TARGET Qt5::WebSockets)
    target_link_libraries(${_target} PRIVATE Qt5::WebSockets)
  endif()

  if(TARGET Qt5::QXcbIntegrationPlugin)
    target_link_libraries(${_target} PRIVATE
      Qt5::QXcbIntegrationPlugin
      Qt5XcbQpa
    )

    if(qtharfbuzz_LIBRARY)
      target_link_libraries(${_target} PRIVATE ${qtharfbuzz_LIBRARY})
    elseif(qtharfbuzzng_LIBRARY)
      target_link_libraries(${_target} PRIVATE ${qtharfbuzzng_LIBRARY})
    endif()

    if(qtpcre2_LIBRARY)
      target_link_libraries(${_target} PRIVATE ${qtpcre2_LIBRARY})
    elseif(qtpcre_LIBRARY)
      target_link_libraries(${_target} PRIVATE ${qtpcre_LIBRARY})
    endif()

    if(qtfreetype_LIBRARY)
      target_link_libraries(${_target} PRIVATE ${qtfreetype_LIBRARY})
    endif()

    if(xcbxkb_LIBRARY)
      target_link_libraries(${_target} PRIVATE ${xcbxkb_LIBRARY})
    endif()

    target_link_libraries(${_target} PRIVATE
      Qt5::QSvgPlugin Qt5::QSvgIconPlugin
      glib-2.0 icuuc icui18n png fontconfig freetype
      GL
      Xi
      xcb-render xcb-image xcb-icccm xcb-sync xcb-xfixes xcb-shm xcb-randr xcb-shape xcb-keysyms xcb-render-util xcb-xinerama xcb
      X11-xcb Xrender Xext X11
      z dl rt
    )
  endif()
    target_link_libraries(${_target} PRIVATE
       m pthread
    )

  if(qmldbg_local_LIBRARY)
    target_link_libraries(${_target} PUBLIC ${qmldbg_local_LIBRARY})
  endif()
  if(qmldbg_tcp_LIBRARY)
    target_link_libraries(${_target} PUBLIC ${qmldbg_tcp_LIBRARY})
  endif()
  endif()
endfunction()
