cmake_minimum_required(VERSION 3.0)
project(iscore_app)

enable_testing()
# TODO strip symbols after deployment build
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib/core")

set(SRCS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
if(APPLE)
        set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/i-score.icns")
        set_source_files_properties(${ICON_SRC} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

if(DEPLOYMENT_BUILD)
    add_executable(${APPNAME} WIN32 MACOSX_BUNDLE ${SRCS} ${ICON_SRC})
else()
    add_executable(${APPNAME} WIN32 ${SRCS})
endif()

target_link_libraries(${APPNAME} iscore_lib_base)

if(ISCORE_STATIC_PLUGINS)
    set(ISCORE_PLUGINS_FILE "${ISCORE_ROOT_BINARY_DIR}/iscore_static_plugins.hpp")
    file(WRITE "${ISCORE_PLUGINS_FILE}" "#pragma once\n#include <QtPlugin>\n")
    foreach(plugin ${ISCORE_PLUGINS_LIST})
        message("Linking statically with i-score plugin : ${plugin}")
        file(APPEND "${ISCORE_PLUGINS_FILE}" "Q_IMPORT_PLUGIN(${plugin})\n")
    endforeach()

    target_link_libraries(${APPNAME} ${ISCORE_PLUGINS_LIST})
endif()


if(ISCORE_STATIC_QT)
    get_target_property(QtCore_LIB Qt5::Core LOCATION)
    get_filename_component(QT_LIB_FOLDER ${QtCore_LIB} DIRECTORY)

    find_library(qtfreetype_LIBRARY NAMES qtfreetype libqtfreetype HINTS ${QT_LIB_FOLDER})
    find_library(Qt5XcbQpa_LIBRARY NAMES Qt5XcbQpa libQt5XcbQpa HINTS ${QT_LIB_FOLDER})
    find_library(Qt5PlatformSupport_LIBRARY NAMES Qt5PlatformSupport libQt5PlatformSupport HINTS ${QT_LIB_FOLDER})
    find_library(qtharfbuzzng_LIBRARY NAMES qtharfbuzzng libqtharfbuzzng HINTS ${QT_LIB_FOLDER})
    find_library(qtpcre_LIBRARY NAMES qtpcre libqtpcre HINTS ${QT_LIB_FOLDER})

    find_library(qsvg_LIBRARY NAMES qsvg libqsvg HINTS ${QT_LIB_FOLDER}/../plugins/imageformats)

    target_link_libraries(
        ${APPNAME}
        Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Svg Qt5::WebSockets Qt5::Xml
        Qt5::QXcbIntegrationPlugin ${Qt5XcbQpa_LIBRARY} ${Qt5PlatformSupport_LIBRARY}
        ${qtharfbuzzng_LIBRARY} ${qtpcre_LIBRARY} ${qtfreetype_LIBRARY} ${qsvg_LIBRARY}
        GL
        Xi
        xcb-render xcb-image xcb-icccm xcb-sync xcb-xfixes xcb-shm xcb-randr xcb-shape xcb-keysyms xcb-xkb xcb-render-util  xcb
        X11-xcb Xrender Xext X11
        z m dl rt pthread
        )
endif()

setup_iscore_common_features(${APPNAME})

install(TARGETS ${APPNAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin)

if(INTEGRATION_TESTING)
    add_library(iscore_integration_lib INTERFACE)

    target_link_libraries(iscore_integration_lib INTERFACE
      ${ISCORE_PLUGINS_LIST}
      Qt5::Core Qt5::Widgets Qt5::Gui Qt5::Network Qt5::Xml Qt5::Svg Qt5::Test)
endif()

include(IScoreDeployment)
