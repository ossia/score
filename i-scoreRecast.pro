#-------------------------------------------------
#
# Project created by QtCreator 2013-10-01T16:30:11
#
#-------------------------------------------------

greaterThan(QT_MAJOR_VERSION, 4) {
    cache()
    QT += widgets # QT includes the core and gui modules by default
}

TEMPLATE = app
TARGET = i-scoreRecast
CONFIG += c++11 warn_on

SOURCES += sources/main.cpp \
    sources/mainwindow.cpp \
    sources/timeevent.cpp \
    sources/engine.cpp \
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
    sources/headerwidget.cpp \
    sources/timebox.cpp \
    sources/scenarioview.cpp \
    sources/state.cpp

HEADERS  += headers/mainwindow.hpp \
    headers/timeevent.hpp \
    headers/engine.hpp \
    headers/graphicsview.hpp \
    headers/utils.hpp \
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
    headers/headerwidget.hpp \
    headers/timebox.hpp \
    headers/scenarioview.hpp \
    headers/state.hpp

RESOURCES += resources/resource.qrc

FORMS    += forms/mainwindow.ui

OTHER_FILES += \
    LICENSE.txt

INCLUDEPATH += headers

android-g++|android-clang{
INCLUDEPATH += /include/libxml2
INCLUDEPATH+=/opt/android-toolchain/include
LIBS += -L/opt/android-toolchain/lib -L/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/
ANDROID_EXTRA_LIBS = \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libAnalysisLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libAudioGraphUtilityLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libAutomation.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libClipper.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libCrossfade.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libDataspaceLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libDictionaryLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libEffectsLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libFFTLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libFilterLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libFunctionLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libGeneratorLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libInterval.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libJamomaAudioGraph.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libJamomaDSP.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libJamomaFoundation.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libJamomaGraph.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libJamomaModular.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libJamomaScore.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libMatrixProcessingLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libMinuit.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libNetworkLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libOSC.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libPlugtastic.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libResamplingLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libScenario.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libSoundfileLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libSpatLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libSystem.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libTrajectoryLib.so \
			/opt/android-toolchain/arm-linux-androideabi/lib/jamoma/libWindowFunctionLib.so
}else{
INCLUDEPATH += /usr/include/libxml2
}

JAMOMA_INCLUDE_PATH=$$(JAMOMA_INCLUDE_PATH)

unix:!macx{
    LIBS += -lJamomaFoundation \
	    -lJamomaDSP \
	    -lJamomaScore \
	    -lJamomaModular

    INCLUDEPATH += \
		$$(JAMOMA_INCLUDE_PATH)/Score/library/tests/ \
		$$(JAMOMA_INCLUDE_PATH)/Modular/library/PeerObject \
		$$(JAMOMA_INCLUDE_PATH)/Modular/library/ProtocolLib \
		$$(JAMOMA_INCLUDE_PATH)/Modular/library/SchedulerLib \
		$$(JAMOMA_INCLUDE_PATH)/DSP/library/includes \
		$$(JAMOMA_INCLUDE_PATH)/Modular/library/includes \
		$$(JAMOMA_INCLUDE_PATH)/Score/library/includes \
		$$(JAMOMA_INCLUDE_PATH)/Foundation/library/includes
}

macx{
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7

    !isEmpty(JAMOMA_INCLUDE_PATH) {
    LIBS += -L/usr/local/jamoma/lib
    LIBS += -lJamomaFoundation \
            -lJamomaDSP \
            -lJamomaScore \
            -lJamomaModular

    INCLUDEPATH += \
                $$(JAMOMA_INCLUDE_PATH)/Score/library/tests/ \
                $$(JAMOMA_INCLUDE_PATH)/Modular/library/PeerObject \
                $$(JAMOMA_INCLUDE_PATH)/Modular/library/ProtocolLib \
                $$(JAMOMA_INCLUDE_PATH)/Modular/library/SchedulerLib \
                $$(JAMOMA_INCLUDE_PATH)/DSP/library/includes \
                $$(JAMOMA_INCLUDE_PATH)/Modular/library/includes \
                $$(JAMOMA_INCLUDE_PATH)/Score/library/includes \
                $$(JAMOMA_INCLUDE_PATH)/Foundation/library/includes

    } else {
    CONFIG += x86_64
    INCLUDEPATH += /usr/local/jamoma/includes
    #LIBS += -L/usr/local/jamoma/lib and -lJamomaFoundation don't work ! Why ??
    LIBS += /usr/local/jamoma/lib/JamomaFoundation.dylib
    LIBS += /usr/local/jamoma/lib/JamomaDSP.dylib
    LIBS += /usr/local/jamoma/lib/JamomaScore.dylib
    LIBS += /usr/local/jamoma/lib/JamomaModular.dylib
    LIBS += -F/Library/Frameworks/ -framework gecode
    }
    QMAKE_CXXFLAGS += -stdlib=libc++ -std=c++11

    QMAKE_LFLAGS += -mmacosx-version-min=$$QMAKE_MACOSX_DEPLOYMENT_TARGET
    QMAKE_LFLAGS += -stdlib=libc++

}

LIBS += -lxml2

