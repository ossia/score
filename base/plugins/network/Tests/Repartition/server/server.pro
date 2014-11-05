TEMPLATE = app
CONFIG += console

QT += core network

QMAKE_CXX = clang-3.5
QMAKE_CXXFLAGS += -std=c++14 -stdlib=libc++
QMAKE_LFLAGS += -lc++ -lpthread
LIBS += -ldns_sd

INCLUDEPATH += ../../API/

#### Libraries ####
  ##  Oscpack  ##
LIBS += /home/jcelerier/git/oscpack/build/liboscpack.a

INCLUDEPATH += /home/jcelerier/git/oscpack/src
DEPENDPATH += /home/jcelerier/git/oscpack/src

unix:!macx: PRE_TARGETDEPS += /home/jcelerier/git/oscpack/build/liboscpack.a

LIBS += /home/jcelerier/work/OSSIA/ScoreNetApi/implementations/build-naiveImpl-Desktop_Qt_5_3_GCC_64bit-Debug/libnaiveImpl.so

#### Source files ####
SOURCES += main.cpp
HEADERS += /home/jcelerier/work/OSSIA/dpetri/src/lib/zeroconf/bonjourserviceregister.h
HEADERS += /home/jcelerier/work/OSSIA/ScoreNetApi/API/net/session/ZeroConfServer.h
HEADERS += /home/jcelerier/work/OSSIA/ScoreNetApi/API/net/session/ZeroConfServerThread.h

