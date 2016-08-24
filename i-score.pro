
TARGET = i-score
TEMPLATE = app
CONFIG += c++14 object_parallel_to_source warn_off

QT+=core widgets gui multimedia network xml opengl websockets quick quickwidgets qml svg

# cp *.h *.hpp from cmake
INCLUDEPATH += $$PWD/base/lib \
$$PWD/3rdparty/nano-signal-slot \
$$PWD/3rdparty/variant/include \
$$PWD/3rdparty/QRecentFilesMenu \
$$PWD/3rdparty/QProgressIndicator \
$$PWD/3rdparty/Qt-Color-Widgets \
$$PWD/3rdparty/Qt-Color-Widgets/QtColorWidgets \
$$PWD/3rdparty/quazip \
$$PWD/3rdparty/quazip/quazip \
$$PWD/API/OSSIA \
$$PWD/API/3rdparty/spdlog/include \
$$PWD/API/3rdparty/oscpack \
$$PWD/API/3rdparty/ModernMIDI \
$$PWD/base/plugins/iscore-lib-state \
$$PWD/base/plugins/iscore-plugin-curve \
$$PWD/base/plugins/iscore-plugin-deviceexplorer \
$$PWD/base/plugins/iscore-plugin-inspector \
$$PWD/base/plugins/iscore-plugin-library \
$$PWD/base/plugins/iscore-plugin-pluginsettings \
$$PWD/base/plugins/iscore-plugin-scenario \
$$PWD/base/plugins/iscore-lib-device \
$$PWD/base/plugins/iscore-lib-process \
$$PWD/base/plugins/iscore-plugin-automation \
$$PWD/base/plugins/iscore-plugin-js \
$$PWD/base/plugins/iscore-plugin-mapping \
$$PWD/base/plugins/iscore-plugin-loop \
$$PWD/base/plugins/iscore-component-executor-automation \
$$PWD/base/plugins/iscore-component-executor-loop \
$$PWD/base/plugins/iscore-component-executor-mapping \
$$PWD/base/plugins/iscore-lib-inspector \
$$PWD/base/plugins/iscore-plugin-engine \
$$PWD/base/plugins/iscore-plugin-midi \
$$PWD/base/plugins/iscore-plugin-recording \
$$PWD/base/plugins/iscore-plugin-interpolation

DEFINES += QT_STATICPLUGIN ISCORE_STATIC_PLUGINS ISCORE_DEPLOYMENT_BUILD

include($$PWD/i-score-srcs.pri)

include($$PWD/3rdparty/QProgressIndicator/qprogressindicator.pri)
include($$PWD/3rdparty/QRecentFilesMenu/QRecentFilesMenu.pri)
include($$PWD/3rdparty/Qt-Color-Widgets/color_widgets.pri)
include($$PWD/3rdparty/quazip/quazip/quazip.pri)

HEADERS += $$PWD/base/app/Application.hpp \
    $$OUT_PWD/iscore_component_executor_automation_plugin.hpp \
    $$OUT_PWD/iscore_component_executor_mapping_plugin.hpp \
    $$OUT_PWD/iscore_component_executor_loop_plugin.hpp
SOURCES += \
    $$PWD/base/app/main.cpp \
    $$PWD/base/app/Application.cpp \
    $$OUT_PWD/iscore_component_executor_automation_plugin.cpp \
    $$OUT_PWD/iscore_component_executor_mapping_plugin.cpp \
    $$OUT_PWD/iscore_component_executor_loop_plugin.cpp

LIBS += /home/jcelerier/travail/build-i-score-Desktop-Défaut/API/OSSIA/libossia.so -lz
