#-------------------------------------------------
#
# Project created by QtCreator 2013-10-01T16:30:11
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = i-scoreRecast
TEMPLATE = app
CONFIG += x86_64
QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7

SOURCES +=\
        sources/mainwindow.cpp \
    sources/graphicstimeevent.cpp \
    sources/engine.cpp \
    sources/graphicstimebox.cpp \
    sources/graphicsview.cpp \
    sources/timeboxsmallview.cpp \
    sources/timeboxfullview.cpp \
    sources/timeboxmodel.cpp \
    sources/test.cpp \
    sources/timeboxheader.cpp \
    sources/timeboxpresenter.cpp \
    sources/timeboxstoreybar.cpp \
    sources/timeboxstorey.cpp \
    sources/pluginview.cpp

HEADERS  += headers/mainwindow.hpp \
    headers/graphicstimeevent.hpp \
    headers/engine.hpp \
    headers/graphicstimebox.hpp \
    headers/graphicsview.hpp \
    headers/itemTypes.hpp \
    headers/timeboxsmallview.hpp \
    headers/timeboxfullview.hpp \
    headers/timeboxmodel.hpp \
    headers/timeboxheader.hpp \
    headers/timeboxpresenter.hpp \
    headers/timeboxstoreybar.hpp \
    headers/timeboxstorey.hpp \
    headers/pluginview.hpp

 RESOURCES += \
     resources/resource.qrc

FORMS    += forms/mainwindow.ui

OTHER_FILES += \
    TODO.txt \
    LICENSE.txt

INCLUDEPATH += headers/ /usr/local/jamoma/includes
QMAKE_LFLAGS += -L/usr/local/jamoma/lib -F/Library/Frameworks/

LIBS += /usr/local/jamoma/lib/JamomaFoundation.dylib
LIBS += /usr/local/jamoma/lib/JamomaDSP.dylib
LIBS += /usr/local/jamoma/lib/JamomaScore.dylib
LIBS += /usr/local/jamoma/lib/JamomaModular.dylib
LIBS += -framework gecode
LIBS += -lxml2

macx-clang {

    QMAKE_CXX = /usr/bin/clang++

    QMAKE_CXXFLAGS += -std=c++11
    QMAKE_CXXFLAGS += -stdlib=libc++
    QMAKE_CXXFLAGS += -mmacosx-version-min=$$QMAKE_MACOSX_DEPLOYMENT_TARGET

    QMAKE_LFLAGS += -stdlib=libc++
     #-F/System/Library/Frameworks/

    INCLUDEPATH += .
   # INCLUDEPATH += /Library/Frameworks/
    INCLUDEPATH += /usr/local/include/libxml2


}
