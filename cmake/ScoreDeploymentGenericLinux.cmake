if(UNIX)

set(SCORE_BIN_INSTALL_DIR ".")

# Qt Libraries
get_target_property(QtCore_LOCATION ${QT_PREFIX}::Core LOCATION)
get_filename_component(QT_SO_DIR ${QtCore_LOCATION} PATH)
# TODO Same than for windows deployment
file(GLOB ICU_SOS "${QT_SO_DIR}/libicu*.so*")
file(GLOB QT_SOS
  "${QT_SO_DIR}/libQt6Core.so*"
  "${QT_SO_DIR}/libQt6Gui.so*"
  "${QT_SO_DIR}/libQt6Widgets.so*"
  "${QT_SO_DIR}/libQt6Network.so*"
  "${QT_SO_DIR}/libQt6Xml.so*"
  "${QT_SO_DIR}/libQt6Svg.so*"
  "${QT_SO_DIR}/libQt6Qml.so*"
  "${QT_SO_DIR}/libQt6OpenGL.so*"
  "${QT_SO_DIR}/libQt6WebSockets.so*"
  "${QT_SO_DIR}/libQt6XcbQpa.so*")

install(FILES
  ${ICU_SOS}
  ${QT_SOS}
  DESTINATION ${SCORE_BIN_INSTALL_DIR})

# Qt conf file
install(
  FILES
    "${CMAKE_CURRENT_LIST_DIR}/Deployment/Linux/qt.conf"
  DESTINATION
    ${SCORE_BIN_INSTALL_DIR})

# Qt Platform Plugin
install(FILES
  "${QT_SO_DIR}/../plugins/platforms/libqxcb.so"
  "${QT_SO_DIR}/../plugins/imageformats/libqsvg.so"
  "${QT_SO_DIR}/../plugins/imageformats/libqjpeg.so"
  "${QT_SO_DIR}/../plugins/generic/libqevdevkeyboardplugin.so"
  "${QT_SO_DIR}/../plugins/generic/libqevdevmouseplugin.so"
  "${QT_SO_DIR}/../plugins/xcbglintegrations/libqxcb-glx-integration.so"

  DESTINATION ${SCORE_BIN_INSTALL_DIR}/plugins/platforms)

# Qt helper script

install(PROGRAMS "${SCORE_SRC}/app/score.sh"
        DESTINATION ".")

endif()
