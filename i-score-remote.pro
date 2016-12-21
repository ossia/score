
TARGET = i-score-remote
TEMPLATE = app
CONFIG += c++14 object_parallel_to_source warn_off
QMAKE_CXXFLAGS += -std=c++14
QT+=core widgets websockets quick quickwidgets qml xml

# cp *.h *.hpp from cmake
INCLUDEPATH += $$PWD/base/lib \
$$PWD/3rdparty/QRecentFilesMenu \
$$PWD/API/3rdparty/brigand \
$$PWD/API/3rdparty/chobo-shl/include \
$$PWD/API/3rdparty/CicmWrapper \
$$PWD/API/3rdparty/fmt \
$$PWD/API/3rdparty/hopscotch-map/src \
$$PWD/API/3rdparty/jni_hpp \
$$PWD/API/3rdparty/ModernMIDI \
$$PWD/API/3rdparty/ModernMIDI/third_party \
$$PWD/API/3rdparty/nano-signal-slot/include \
$$PWD/API/3rdparty/oscpack \
$$PWD/API/3rdparty/pure-data \
$$PWD/API/3rdparty/rapidjson/include \
$$PWD/API/3rdparty/readerwriterqueue \
$$PWD/API/3rdparty/spdlog/include \
$$PWD/API/3rdparty/variant/include \
$$PWD/API/OSSIA \
$$PWD/API/3rdparty/spdlog/include \
$$PWD/API/3rdparty/oscpack \
$$PWD/API/3rdparty/ModernMIDI \
$$PWD/API/3rdparty/rapidjson/include \
$$PWD/API/3rdparty/ModernMIDI/third_party \
$$PWD/base/plugins/iscore-lib-state \
$$PWD/base/plugins/iscore-lib-device \
$$PWD/base/remote \
$$PWD/build

include($$PWD/3rdparty/QRecentFilesMenu/QRecentFilesMenu.pri)
DEFINES += QT_STATICPLUGIN ISCORE_STATIC_PLUGINS ISCORE_DEPLOYMENT_BUILD RAPIDJSON_HAS_STDSTRING

include($$PWD/i-score-remote-srcs.pri)
RESOURCES += $$PWD/base/remote/qml.qrc
SOURCES += $$PWD/base/remote/main.cpp

LIBS += -lz
INCLUDEPATH += /opt/boost_1_62_0
ios{
    QMAKE_INFO_PLIST = $$PWD/base/app/Info.plist.ios
}
