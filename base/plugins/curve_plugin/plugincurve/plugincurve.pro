#-------------------------------------------------
#
# Project created by QtCreator 2014-07-25T14:32:48
#
#-------------------------------------------------

TARGET = plugincurve
DESTDIR = ../plugincurve/lib
QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
TEMPLATE = lib
CONFIG += staticlib c++11 warn_on
QMAKE_CXXFLAGS += -std=c++11
INCLUDEPATH += include
SOURCES +=  src/plugincurve.cpp\
            src/plugincurvemodel.cpp \
            src/plugincurvepresenter.cpp \
            src/plugincurveview.cpp \
            src/plugincurvepoint.cpp \
            src/plugincurvesection.cpp \
            src/plugincurvesectionbezier.cpp \
            src/plugincurvesectionlinear.cpp \
            src/plugincurvemap.cpp \
            src/plugincurvegrid.cpp \
            src/plugincurvemenupoint.cpp \
            src/plugincurvemenusection.cpp \
            src/plugincurvezoomer.cpp

HEADERS += include/plugincurve.hpp\
           include/plugincurve2_global.hpp\
           include/plugincurvemodel.hpp \
           include/plugincurvepresenter.hpp \
           include/plugincurveview.hpp \
           include/plugincurvepoint.hpp \
           include/plugincurvesection.hpp \
           include/plugincurvesectionbezier.hpp \
           include/plugincurvesectionlinear.hpp \
           include/plugincurvemap.hpp \
           include/plugincurvegrid.hpp \
           include/plugincurvemenupoint.h \
           include/plugincurvemenusection.hpp \
           include/plugincurvezoomer.hpp

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}
