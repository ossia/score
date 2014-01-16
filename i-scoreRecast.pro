#-------------------------------------------------
#
# Project created by QtCreator 2013-10-01T16:30:11
#
#-------------------------------------------------

cache()

# QT includes the core and gui modules by default
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = app
TARGET = i-scoreRecast
CONFIG += x86_64 warn_on qt
QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7

SOURCES += sources/main.cpp \
    sources/mainwindow.cpp \
    sources/timeevent.cpp \
    sources/engine.cpp \
    sources/graphicstimebox.cpp \
    sources/graphicsview.cpp \
    sources/timeboxsmallview.cpp \
    sources/timeboxfullview.cpp \
    sources/timeboxmodel.cpp \
    sources/timeboxheader.cpp \
    sources/timeboxpresenter.cpp \
    sources/timeboxstoreybar.cpp \
    sources/timeboxstorey.cpp \
    sources/pluginview.cpp \
    sources/pixmapbutton.cpp \
    sources/automationview.cpp \
    sources/headerwidget.cpp

HEADERS  += headers/mainwindow.hpp \
    headers/timeevent.hpp \
    headers/engine.hpp \
    headers/graphicstimebox.hpp \
    headers/graphicsview.hpp \
    headers/itemtypes.hpp \
    headers/timeboxsmallview.hpp \
    headers/timeboxfullview.hpp \
    headers/timeboxmodel.hpp \
    headers/timeboxheader.hpp \
    headers/timeboxpresenter.hpp \
    headers/timeboxstoreybar.hpp \
    headers/timeboxstorey.hpp \
    headers/pluginview.hpp \
    headers/pixmapbutton.hpp \
    headers/automationview.hpp \
    headers/headerwidget.hpp

RESOURCES += resources/resource.qrc

FORMS    += forms/mainwindow.ui

OTHER_FILES += \
    TODO.txt \
    LICENSE.txt

INCLUDEPATH += headers
INCLUDEPATH += /usr/local/jamoma/includes
INCLUDEPATH += /usr/local/include/libxml2

#LIBS += -L/usr/local/jamoma/lib and -lJamomaFoundation don't work ! Why ??
LIBS += /usr/local/jamoma/lib/JamomaFoundation.dylib
LIBS += /usr/local/jamoma/lib/JamomaDSP.dylib
LIBS += /usr/local/jamoma/lib/JamomaScore.dylib
LIBS += /usr/local/jamoma/lib/JamomaModular.dylib
LIBS += -F/Library/Frameworks/ -framework gecode
LIBS += -lxml2

QMAKE_CXXFLAGS += -std=c++11
QMAKE_CXXFLAGS += -stdlib=libc++

QMAKE_LFLAGS += -mmacosx-version-min=$$QMAKE_MACOSX_DEPLOYMENT_TARGET
QMAKE_LFLAGS += -stdlib=libc++


