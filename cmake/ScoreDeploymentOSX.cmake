if(APPLE)

set_target_properties(
  ${APPNAME}
  PROPERTIES
    MACOSX_BUNDLE_INFO_STRING "ossia score, an interactive sequencer for the intermedia arts"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.ossia.score"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${SCORE_VERSION}"
    MACOSX_BUNDLE_BUNDLE_NAME "score"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${SCORE_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${SCORE_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "ossia.io"
    MACOSX_BUNDLE_ICON_FILE "score.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)

# Copy our dylibs if necessary
if(NOT SCORE_STATIC_PLUGINS)
    set(SCORE_BUNDLE_PLUGINS_FOLDER "${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/")

    function(score_copy_osx_plugin theTarget)
      add_custom_command(
        TARGET ${APPNAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${theTarget}> ${SCORE_BUNDLE_PLUGINS_FOLDER})
    endfunction()

    # Copy score plugins into the app bundle
    add_custom_command(TARGET ${APPNAME} POST_BUILD
                       COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/)

    foreach(plugin ${SCORE_PLUGINS_LIST})
      score_copy_osx_plugin(${plugin})
    endforeach()
endif()

# set-up Qt stuff.
# Remember to set CMAKE_INSTALL_PREFIX on the CMake command line.
get_target_property(QT_LIBRARY_FILE ${QT_PREFIX}::Core LOCATION)
if("${QT_LIBRARY_FILE}" MATCHES "\.a$")
    set(QT_STATIC 1)
else()
    set(QT_STATIC 0)
endif()

get_filename_component(QT_LIBRARY_DIR ${QT_LIBRARY_FILE} PATH)
get_filename_component(QT_LIBRARY_DIR "${QT_LIBRARY_DIR}/.." ABSOLUTE)
set(QT_PLUGINS_DIR "${Qt5Widgets_DIR}/../../../plugins")
set(QT_QML_PLUGINS_DIR "${Qt5Widgets_DIR}/../../../qml")

set(plugin_dest_dir "${APPNAME}.app/Contents/PlugIns")
set(qtconf_dest_dir "${APPNAME}.app/Contents/Resources")
set(qml_dest_dir "${APPNAME}.app/Contents/Resources/qml")

# If we are in a dynamic build of qt
if(EXISTS "${QT_PLUGINS_DIR}/platforms/libqcocoa.dylib")
  install(FILES "${QT_PLUGINS_DIR}/platforms/libqcocoa.dylib" DESTINATION "${plugin_dest_dir}/platforms")
  install(FILES "${QT_PLUGINS_DIR}/imageformats/libqsvg.dylib" DESTINATION "${plugin_dest_dir}/imageformats")
  install(FILES "${QT_PLUGINS_DIR}/iconengines/libqsvgicon.dylib" DESTINATION "${plugin_dest_dir}/iconengines")
endif()

# Note: freetype still seems to cause crashes on macOS
# install(CODE "
#     file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${qtconf_dest_dir}/qt.conf\" \"[Paths]
# Plugins = PlugIns
# Qml2Imports = Resources/qml
#
# [Platforms]
# CocoaArguments=fontengine=freetype
# \")
# "
# COMPONENT OssiaScore
# )

# set-up Faust stuff
if(EXISTS "${CMAKE_BINARY_DIR}/src/plugins/score-plugin-faust/faustlibs-prefix/src/faustlibs")
  install(
    DIRECTORY
      "${CMAKE_BINARY_DIR}/src/plugins/score-plugin-faust/faustlibs-prefix/src/faustlibs/"
    DESTINATION
      "${APPNAME}.app/Contents/Resources/Faust"
    COMPONENT OssiaScore
      PATTERN ".git" EXCLUDE
      PATTERN "doc" EXCLUDE
      PATTERN "*.html" EXCLUDE
      PATTERN "*.svg" EXCLUDE
      PATTERN "*.scad" EXCLUDE
      PATTERN "*.obj" EXCLUDE
      PATTERN "build" EXCLUDE
      PATTERN ".gitignore" EXCLUDE
      PATTERN "modalmodels" EXCLUDE
   )
endif()

if(QT_STATIC)
    install(CODE "
        set(BU_CHMOD_BUNDLE_ITEMS ON)
        include(BundleUtilities)
        fixup_bundle(
          \"\${CMAKE_INSTALL_PREFIX}/${APPNAME}.app\"
          \"\"
          \"\")

        " COMPONENT OssiaScore)

elseif(SCORE_STATIC_PLUGINS)
    install(CODE "
        file(GLOB_RECURSE QTPLUGINS \"\${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/*.dylib\")
        file(GLOB_RECURSE QMLPLUGINS \"\${CMAKE_INSTALL_PREFIX}/${qml_dest_dir}/*.dylib\")

        set(BU_CHMOD_BUNDLE_ITEMS ON)
        include(BundleUtilities)
        fixup_bundle(
          \"\${CMAKE_INSTALL_PREFIX}/${APPNAME}.app\"
          \"\${QTPLUGINS};\${QMLPLUGINS}\"
          \"${QT_LIBRARY_DIR}\")

      execute_process(COMMAND
                \"${SCORE_ROOT_SOURCE_DIR}/cmake/Deployment/OSX/set_rpath_static.sh\"
                \"${CMAKE_INSTALL_PREFIX}/score.app/Contents\")
        " COMPONENT OssiaScore)
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
            \"\${CMAKE_INSTALL_PREFIX}/${qml_dest_dir}/*.dylib\")
        set(BU_CHMOD_BUNDLE_ITEMS ON)
        include(BundleUtilities)
        fixup_bundle(
           \"${CMAKE_INSTALL_PREFIX}/score.app\"
           \"\${QTPLUGINS};\${QMLPLUGINS};${SCORE_BUNDLE_INSTALLED_PLUGINS}\"
       \"${QT_LIBRARY_DIR};${CMAKE_BINARY_DIR}/plugins;${CMAKE_INSTALL_PREFIX}/plugins;${CMAKE_BINARY_DIR}/3rdparty/libossia/src;${CMAKE_BINARY_DIR}/src/lib;${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/\"
        )
message(\"${SCORE_ROOT_SOURCE_DIR}/cmake/Deployment/OSX/set_rpath.sh\"
          \"${CMAKE_INSTALL_PREFIX}/score.app/Contents\")
execute_process(COMMAND
          \"${SCORE_ROOT_SOURCE_DIR}/cmake/Deployment/OSX/set_rpath.sh\"
          \"${CMAKE_INSTALL_PREFIX}/score.app/Contents\")
      ")
endif()

set(CPACK_GENERATOR "DragNDrop")

endif()
