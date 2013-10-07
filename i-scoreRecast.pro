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

SOURCES += main.cpp\
        mainwindow.cpp \
    graphicstimeevent.cpp \
    engine.cpp \
    graphicstimeprocess.cpp

HEADERS  += mainwindow.hpp \
    graphicstimeevent.hpp \
    engine.hpp \
    graphicstimeprocess.hpp

FORMS    += mainwindow.ui

OTHER_FILES += \
    TODO.txt

    INCLUDEPATH += /usr/local/jamoma/includes
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
