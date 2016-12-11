QT += qml quick

CONFIG += c++14

HEADERS += WidgetListModel.hpp \
    centralitemmodel.h \
    nodemodel.h
SOURCES += main.cpp WidgetListModel.cpp \
    centralitemmodel.cpp \
    nodemodel.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
