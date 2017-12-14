if(APPLE)

set_target_properties(
  ${APPNAME}
  PROPERTIES
    MACOSX_BUNDLE_INFO_STRING "score, an interactive sequencer for the intermedia arts"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.score"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${SCORE_VERSION}"
    MACOSX_BUNDLE_BUNDLE_NAME "score"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${SCORE_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${SCORE_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "The score team"
    MACOSX_BUNDLE_ICON_FILE "score.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)

if(TARGET ${APPNAME}_unity)
set_target_properties(
  ${APPNAME}_unity
  PROPERTIES
    MACOSX_BUNDLE_INFO_STRING "score, an interactive sequencer for the intermedia arts"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.score"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${SCORE_VERSION}"
    MACOSX_BUNDLE_BUNDLE_NAME "score"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${SCORE_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${SCORE_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "The score team"
    MACOSX_BUNDLE_ICON_FILE "score.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)
endif()
# Copy our dylibs if necessary
if(NOT SCORE_STATIC_PLUGINS)
    set(SCORE_BUNDLE_PLUGINS_FOLDER "${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/")
    
    function(score_copy_osx_plugin theTarget)
      add_custom_command(
        TARGET ${APPNAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${theTarget}> ${SCORE_BUNDLE_PLUGINS_FOLDER})

      if(TARGET ${theTarget}_unity)
        add_custom_command(
          TARGET ${APPNAME}_unity POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${theTarget}_unity> ${SCORE_BUNDLE_PLUGINS_FOLDER})
      endif()
    endfunction()

    # Copy score plugins into the app bundle
    add_custom_command(TARGET ${APPNAME} POST_BUILD
                       COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/)
  if(TARGET ${APPNAME}_unity)
    add_custom_command(TARGET ${APPNAME}_unity POST_BUILD
               COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/)
  endif()

    foreach(plugin ${SCORE_PLUGINS_LIST})
      score_copy_osx_plugin(${plugin})
    endforeach()
endif()

# set-up Qt stuff.
# Remember to set CMAKE_INSTALL_PREFIX on the CMake command line.
get_target_property(QT_LIBRARY_DIR Qt5::Core LOCATION)
get_filename_component(QT_LIBRARY_DIR ${QT_LIBRARY_DIR} PATH)
get_filename_component(QT_LIBRARY_DIR "${QT_LIBRARY_DIR}/.." ABSOLUTE)
set(QT_PLUGINS_DIR "${Qt5Widgets_DIR}/../../../plugins")
set(QT_QML_PLUGINS_DIR "${Qt5Widgets_DIR}/../../../qml")

set(plugin_dest_dir "${APPNAME}.app/Contents/PlugIns")
set(qtconf_dest_dir "${APPNAME}.app/Contents/Resources")
set(qml_dest_dir "${APPNAME}.app/Contents/Resources/qml")

install(FILES "${QT_PLUGINS_DIR}/platforms/libqcocoa.dylib" DESTINATION "${plugin_dest_dir}/platforms")
install(FILES "${QT_PLUGINS_DIR}/imageformats/libqsvg.dylib" DESTINATION "${plugin_dest_dir}/imagesformats")
install(FILES "${QT_PLUGINS_DIR}/iconengines/libqsvgicon.dylib" DESTINATION "${plugin_dest_dir}/iconengines")
install(FILES "${QT_QML_PLUGINS_DIR}/QtQuick.2/libqtquick2plugin.dylib" DESTINATION "${plugin_dest_dir}/quick")
install(DIRECTORY "${QT_QML_PLUGINS_DIR}/QtQuick" "${QT_QML_PLUGINS_DIR}/QtQuick.2" DESTINATION "${qml_dest_dir}")

install(CODE "
    file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${qtconf_dest_dir}/qt.conf\" \"[Paths]
Plugins = PlugIns
Qml2Imports = Resources/qml
\")
" )
#Translations = Resources/translations
#Data = Resources

if(SCORE_STATIC_PLUGINS)
    install(CODE "
        file(GLOB_RECURSE QTPLUGINS
            \"\${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/*.dylib\")
        file(GLOB_RECURSE QMLPLUGINS
            \"\${CMAKE_INSTALL_PREFIX}/${qml_plugin_dest_dir}/*.dylib\")
        set(BU_CHMOD_BUNDLE_ITEMS ON)
        include(BundleUtilities)
        fixup_bundle(
          \"\${CMAKE_INSTALL_PREFIX}/${APPNAME}.app\"
          \"\${QTPLUGINS};${QMLPLUGINS}\"
          \"${QT_LIBRARY_DIR}\")

      execute_process(COMMAND
                \"${SCORE_ROOT_SOURCE_DIR}/CMake/Deployment/OSX/set_rpath_static.sh\"
                \"${CMAKE_INSTALL_PREFIX}/score.app/Contents\")
        " COMPONENT Runtime)
else()
    set(CMAKE_INSTALL_RPATH "plugins")
    foreach(plugin ${SCORE_PLUGINS_LIST})
        list(APPEND SCORE_BUNDLE_INSTALLED_PLUGINS "${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/lib${plugin}.dylib")
    endforeach()

    install(CODE "
      message(${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins)
        file(GLOB_RECURSE QTPLUGINS
            \"\${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/*.dylib\")
        file(GLOB_RECURSE QMLPLUGINS
            \"\${CMAKE_INSTALL_PREFIX}/${qml_plugin_dest_dir}/*.dylib\")
        set(BU_CHMOD_BUNDLE_ITEMS ON)
        include(BundleUtilities)
        fixup_bundle(
           \"${CMAKE_INSTALL_PREFIX}/score.app\"
           \"\${QTPLUGINS};${QMLPLUGINS};${SCORE_BUNDLE_INSTALLED_PLUGINS}\"
       \"${QT_LIBRARY_DIR};${CMAKE_BINARY_DIR}/plugins;${CMAKE_INSTALL_PREFIX}/plugins;${CMAKE_BINARY_DIR}/API/OSSIA;${CMAKE_BINARY_DIR}/base/lib;${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/\"
        )
message(\"${SCORE_ROOT_SOURCE_DIR}/CMake/Deployment/OSX/set_rpath.sh\"
          \"${CMAKE_INSTALL_PREFIX}/score.app/Contents\")
execute_process(COMMAND
          \"${SCORE_ROOT_SOURCE_DIR}/CMake/Deployment/OSX/set_rpath.sh\"
          \"${CMAKE_INSTALL_PREFIX}/score.app/Contents\")
      ")
endif()

set(CPACK_GENERATOR "DragNDrop")

endif()
