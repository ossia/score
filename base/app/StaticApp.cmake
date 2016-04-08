if(ISCORE_STATIC_QT)
    find_package(Qt5 5.3 REQUIRED COMPONENTS DBus Qml Quick)

    get_target_property(QtCore_LIB Qt5::Core LOCATION)
    get_filename_component(QT_LIB_FOLDER ${QtCore_LIB} DIRECTORY)

    find_library(qtharfbuzzng_LIBRARY NAMES qtharfbuzzng libqtharfbuzzng HINTS ${QT_LIB_FOLDER})
    find_library(qtfreetype_LIBRARY NAMES qtfreetype libqtfreetype HINTS ${QT_LIB_FOLDER})
    find_library(Qt5XcbQpa_LIBRARY NAMES Qt5XcbQpa libQt5XcbQpa HINTS ${QT_LIB_FOLDER})
    find_library(Qt5PlatformSupport_LIBRARY NAMES Qt5PlatformSupport libQt5PlatformSupport HINTS ${QT_LIB_FOLDER})
    find_library(qtpcre_LIBRARY NAMES qtpcre libqtpcre HINTS ${QT_LIB_FOLDER})

    find_library(qsvg_LIBRARY NAMES qsvg libqsvg HINTS ${QT_LIB_FOLDER}/../plugins/imageformats)
    find_library(xcbxkb_LIBRARY NAMES xcb-xkb libxcb-xkb)

    find_library(qmldbg_local_LIBRARY NAMES qmldbg_local HINTS ${QT_LIB_FOLDER}/../plugins/qmltooling)

    add_library(Qt5PlatformSupport STATIC IMPORTED)    
    set_target_properties(Qt5PlatformSupport PROPERTIES IMPORTED_LOCATION ${Qt5PlatformSupport_LIBRARY})
    set_property(TARGET Qt5PlatformSupport PROPERTY INTERFACE_LINK_LIBRARIES Qt5::Gui Qt5::DBus dbus-1 icudata m dl gthread-2.0 Xrender Xext X11 udev GL)

    add_library(Qt5XcbQpa STATIC IMPORTED)
    set_target_properties(Qt5XcbQpa PROPERTIES IMPORTED_LOCATION ${Qt5XcbQpa_LIBRARY})
    set_property(TARGET Qt5XcbQpa PROPERTY INTERFACE_LINK_LIBRARIES Qt5PlatformSupport)

    message("${Qt5Qml_PLUGINS}")
    target_link_libraries(
        ${APPNAME} PRIVATE
        Qt5::Core Qt5::Gui Qt5::Widgets Qt5::DBus Qt5::Network Qt5::Svg Qt5::Xml Qt5::Qml ${Qt5Qml_PLUGINS} ${Qt5Qml_PLUGINS} Qt5::Quick Qt5PlatformSupport
    )

    if(TARGET Qt5::WebSockets)
      target_link_libraries(${APPNAME} PRIVATE Qt5::WebSockets)
    endif()

    if(TARGET Qt5::QXcbIntegrationPlugin)
      target_link_libraries(${APPNAME} PRIVATE
        Qt5::QXcbIntegrationPlugin
        Qt5XcbQpa
      )

      if(qtharfbuzzng_LIBRARY)
        target_link_libraries(${APPNAME} PRIVATE ${qtharfbuzzng_LIBRARY})
      endif()
      if(qtpcre_LIBRARY)
        target_link_libraries(${APPNAME} PRIVATE ${qtpcre_LIBRARY})
      endif()
      if(qtfreetype_LIBRARY)
        target_link_libraries(${APPNAME} PRIVATE ${qtfreetype_LIBRARY})
      endif()
      if(xcbxkb_LIBRARY)
        target_link_libraries(${APPNAME} PRIVATE ${xcbxkb_LIBRARY})
      endif()

      target_link_libraries(${APPNAME} PRIVATE
        ${qsvg_LIBRARY}
        glib-2.0 icuuc icui18n png fontconfig freetype
        GL
        Xi
        xcb-render xcb-image xcb-icccm xcb-sync xcb-xfixes xcb-shm xcb-randr xcb-shape xcb-keysyms xcb-render-util xcb-xinerama xcb
        X11-xcb Xrender Xext X11
        z dl rt
      )
    endif()
      target_link_libraries(${APPNAME} PRIVATE
         m pthread
      )

    target_link_libraries(${APPNAME} PUBLIC ${qmldbg_local_LIBRARY})
endif()
