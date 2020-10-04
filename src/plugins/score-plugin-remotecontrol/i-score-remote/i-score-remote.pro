TEMPLATE = app

QT += qml quick widgets websockets

CONFIG += c++11
HEADERS += RemoteApplication.hpp
SOURCES += main.cpp RemoteApplication.hpp RemoteApplication.cpp

RESOURCES += remote.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

DISTFILES += \
    AndroidManifest.xml
