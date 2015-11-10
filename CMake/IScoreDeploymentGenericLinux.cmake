if(UNIX)

set(ISCORE_BIN_INSTALL_DIR ".")

find_package(Jamoma)
if(${Jamoma_FOUND})
function(copy_in_bundle_jamoma_linux theTarget theBundle theLibs thePlugins)
# TODO use OSX bundle name property?
    # Jamoma setup
    add_custom_command(TARGET ${theTarget} POST_BUILD
                      COMMAND mkdir -p ${theBundle}/Contents/Frameworks/jamoma/extensions)

    foreach(library ${theLibs})
        message("${library}")
        add_custom_command(TARGET ${theTarget} POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Jamoma::${library}> ${theBundle}/Contents/Frameworks/jamoma)
    endforeach()
    foreach(plugin ${thePlugins})

        message("${plugin}")
        add_custom_command(TARGET ${theTarget} POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Jamoma::${plugin}> ${theBundle}/Contents/Frameworks/jamoma/extensions)
    endforeach()

endfunction()


    copy_in_bundle_jamoma_linux(${APPNAME} ${ISCORE_BIN_INSTALL_DIR} "${JAMOMA_LIBS}" "${JAMOMA_PLUGINS}")
endif()

# Qt Libraries

get_target_property(QtCore_LOCATION Qt5::Core LOCATION)
get_filename_component(QT_SO_DIR ${QtCore_LOCATION} PATH)
file(GLOB ICU_SOS "${QT_SO_DIR}/libicu*.so*")
file(GLOB QT_SOS
  "${QT_SO_DIR}/libQt5Core.so*"
  "${QT_SO_DIR}/libQt5Gui.so*"
  "${QT_SO_DIR}/libQt5Widgets.so*"
  "${QT_SO_DIR}/libQt5Network.so*"
  "${QT_SO_DIR}/libQt5Xml.so*"
  "${QT_SO_DIR}/libQt5PrintSupport.so*"
  "${QT_SO_DIR}/libQt5Svg.so*"
  "${QT_SO_DIR}/libQt5Qml.so*"
  "${QT_SO_DIR}/libQt5OpenGL.so*"
  "${QT_SO_DIR}/libQt5WebSockets.so*"
  "${QT_SO_DIR}/libQt5XcbQpa.so*"
  "${QT_SO_DIR}/libQt5Test.so*")

install(FILES
  ${ICU_SOS}
  ${QT_SOS}
  DESTINATION ${ISCORE_BIN_INSTALL_DIR})

# Qt conf file
install(
  FILES
    "${CMAKE_CURRENT_LIST_DIR}/Deployment/Linux/qt.conf"
  DESTINATION
    ${ISCORE_BIN_INSTALL_DIR})

# Qt Platform Plugin
install(FILES
  "${QT_SO_DIR}/../plugins/platforms/libqxcb.so"
  "${QT_SO_DIR}/../plugins/imageformats/libqsvg.so"
  "${QT_SO_DIR}/../plugins/imageformats/libqjpeg.so"
  "${QT_SO_DIR}/../plugins/generic/libqevdevkeyboardplugin.so"
  "${QT_SO_DIR}/../plugins/generic/libqevdevmouseplugin.so"
  "${QT_SO_DIR}/../plugins/xcbglintegrations/libqxcb-glx-integration.so"

  DESTINATION ${ISCORE_BIN_INSTALL_DIR}/plugins/platforms)

# Qt helper script

install(PROGRAMS "${CMAKE_SOURCE_DIR}/base/app/i-score.sh"
        DESTINATION "."
        COMPONENT DynamicRuntimeHelper)

endif()
