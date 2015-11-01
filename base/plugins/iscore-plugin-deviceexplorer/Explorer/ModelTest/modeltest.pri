#Boris: hand made from modeltest.pro
# just added path before each source file
# and removed TARGET & ModelTest/tst_modeltest.cpp from SOURCES

#CONFIG += testcase
#TARGET = tst_modeltest
QT += widgets testlib
SOURCES         += ModelTest/modeltest.cpp ModelTest/dynamictreemodel.cpp
HEADERS         += ModelTest/modeltest.h ModelTest/dynamictreemodel.h



DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
