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
    file(WRITE "${ISCORE_PLUGINS_FILE}" "#pragma once\n")
    foreach(plugin ${ISCORE_PLUGINS_LIST})
        message("${plugin}")
        file(APPEND "${ISCORE_PLUGINS_FILE}" "Q_IMPORT_PLUGIN(${plugin})\n")
    endforeach()

    target_link_libraries(${APPNAME} ${ISCORE_PLUGINS_LIST})
endif()

if(ISCORE_STATIC_QT)
find_library(qtfreetype_LIBRARY NAMES qtfreetype libqtfreetype HINTS /opt/qt5-static/lib)
find_library(Qt5XcbQpa_LIBRARY NAMES Qt5XcbQpa libQt5XcbQpa HINTS /opt/qt5-static/lib)
find_library(Qt5PlatformSupport_LIBRARY NAMES Qt5PlatformSupport libQt5PlatformSupport HINTS /opt/qt5-static/lib)
find_library(qtharfbuzzng_LIBRARY NAMES qtharfbuzzng libqtharfbuzzng HINTS /opt/qt5-static/lib)
find_library(qtpcre_LIBRARY NAMES qtpcre libqtpcre HINTS /opt/qt5-static/lib)
find_library(xcb_static_LIBRARY NAMES xcb-static libxcb-static HINTS /opt/qt5-static/lib)

find_library(qxcb_glx_integration_LIBRARY NAMES qxcb-glx-integration libqxcb-glx-integration HINTS /opt/qt5-static/plugins/xcbglintegrations)

find_library(qminimal_LIBRARY NAMES qminimal libqminimal HINTS /opt/qt5-static/plugins/platforms)
find_library(qxcb_LIBRARY NAMES qxcb libqxcb HINTS /opt/qt5-static/plugins/platforms)


find_library(qsvg_LIBRARY NAMES qsvg libqsvg HINTS /opt/qt5-static/plugins/imageformats)
target_link_libraries(
    ${APPNAME}
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Svg Qt5::WebSockets Qt5::Xml
    Qt5::PrintSupport Qt5::Test
    ${Qt5Gui_PLUGINS} ${Qt5XcbQpa_LIBRARY} ${Qt5PlatformSupport_LIBRARY}
    ${qtharfbuzzng_LIBRARY} ${qtpcre_LIBRARY} ${qminimal_LIBRARY}  ${qtfreetype_LIBRARY}
    ${GLIB2_LIBRARY} GL
#    Qt5PlatformSupport   ${xcb_static_LIBRARY}
#    ${qxcb_LIBRARY} ${qsvg_LIBRARY}
#    GL
#    xcb-glx
    Xi xcb-render xcb-image xcb-icccm xcb-sync xcb-xfixes xcb-shm xcb-randr xcb-shape xcb-keysyms xcb-xkb xcb-render-util X11-xcb xcb  Xrender Xext X11 z m dl rt pthread

# qwayland-egl wayland-egl qwayland-generic Qt5WaylandClient xkbcommon wayland-client wayland-cursor qxcb qxcb-egl-integration qxcb-glx-integration xcb-glx Qt5XcbQpa Xi SM ICE xcb-render xcb-image xcb-icccm xcb-sync xcb-xfixes xcb-shm xcb-randr xcb-shape xcb-keysyms qdds qicns qico qjp2 qmng qtga qtiff qwbmp qwebp qeglfs-x11-integration X11-xcb xcb Qt5EglDeviceIntegration Qt5PlatformSupport fontconfig freetype Qt5DBus Xrender Xext X11 Qt5Gui png qtharfbuzzng Qt5Core z icui18n icuuc icudata qtpcre m dl rt EGL GL pthread)
)
endif()
setup_iscore_common_features(${APPNAME})

install(TARGETS ${APPNAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
        COMPONENT Runtime)

if(INTEGRATION_TESTING)
    add_library(iscore_integration_lib INTERFACE)

    target_link_libraries(iscore_integration_lib INTERFACE
      ${ISCORE_PLUGINS_LIST}
	  Qt5::Core Qt5::Widgets Qt5::Gui Qt5::Network Qt5::Xml Qt5::PrintSupport Qt5::Svg Qt5::Test)
endif()

include(IScoreDeployment)
