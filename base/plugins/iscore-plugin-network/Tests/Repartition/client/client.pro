TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
QT += core network

QMAKE_CXXFLAGS += -std=c++14
QMAKE_LFLAGS += -lpthread

#QMAKE_CXX = clang-3.5
#QMAKE_CXXFLAGS += -std=c++14 -stdlib=libc++
#QMAKE_LFLAGS += -lc++ -lpthread

INCLUDEPATH += ../../API/

#### Libraries ####
  ##  Oscpack  ##
LIBS += /home/jcelerier/git/oscpack/build/liboscpack.a

INCLUDEPATH += /home/jcelerier/git/oscpack/src
DEPENDPATH += /home/jcelerier/git/oscpack/src

unix:!macx: PRE_TARGETDEPS += /home/jcelerier/git/oscpack/build/liboscpack.a


LIBS += -ldns_sd

LIBS += /home/jcelerier/work/OSSIA/ScoreNetApi/implementations/build-naiveImpl-Desktop_Qt_5_3_GCC_64bit-Debug/libnaiveImpl.so
#### Source files ####
SOURCES += main.cpp
message($$PWD)
HEADERS +=  /home/jcelerier/work/OSSIA/dpetri/src/lib/zeroconf/bonjourserviceresolver.h
HEADERS +=  /home/jcelerier/work/OSSIA/dpetri/src/lib/zeroconf/bonjourservicebrowser.h
HEADERS +=  /home/jcelerier/work/OSSIA/ScoreNetApi/API/net/session/ZeroConfClient.h
HEADERS +=  /home/jcelerier/work/OSSIA/ScoreNetApi/API/net/session/ZeroConfClientThread.h
