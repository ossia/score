#BUILD INSTRUCTIONS
## Dependencies
To build, you will need the following dependencies : 
 * [qt5](http://www.qt.io/) (>= 5.3)
 * [cmake](https://cmake.org/) (>= 3.1)
 * [boost](http://www.boost.org/) (>= 1.55)
 * [jamoma core](https://github.com/jamoma/JamomaCore) (_project compile without, but it's required by the API used for execution ..._)

Your compiler need to be C++11 compliant (gcc5.x should work).

## Debian-like systems

    $ sudo apt-get install cmake qtbase5-dev qtdeclarative5-dev qttools5-dev libqt5svg5-dev libboost-dev build-essentials g++
    $ mkdir -p build_folder
    $ cd build_folder
    $ cmake path/to/i-score/repo
    $ make -j4
_see below for CMake options_
### run :
 
    $ ./i-score

## OSX :

    brew install qt5 boost
    mkdir build
    cd build

    ISCORE_CMAKE_QT_CONFIG="$(find /usr/local/Cellar/qt5 -name Qt5Config.cmake)"
    ISCORE_CMAKE_QT_PATH="$(dirname $(dirname $ISCORE_CMAKE_QT_CONFIG))"
    cmake -DCMAKE_PREFIX_PATH="$ISCORE_CMAKE_QT_PATH/Qt5;$ISCORE_CMAKE_QT_PATH/Qt5Widgets;$ISCORE_CMAKE_QT_PATH/Qt5Network;$ISCORE_CMAKE_QT_PATH/Qt5Test;$ISCORE_CMAKE_QT_PATH/Qt5Gui;$ISCORE_CMAKE_QT_PATH/Qt5Xml;$ISCORE_CMAKE_QT_PATH/Qt5Core;" ..
    make all_unity -j4

### To make a deployable .app

    cmake -DCOTIRE_MINIMUM_NUMBER_OF_TARGET_SOURCES=5000 -DCMAKE_BUILD_TYPE=Release -DDEPLOYMENT_BUILD:Bool=True -DCMAKE_PREFIX_PATH="$ISCORE_CMAKE_QT_PATH/Qt5;/usr/local/jamoma/share/cmake" -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle ~/i-score

### run :
 
   $ ./i-score

## Build on windows (**change paths**)

First install boost, Qt (MinGW for now), CMake
Then in QtCreator run CMake with : 

    -DBoost_INCLUDE_DIR=c:\boost_1_57_0

other cmake options : 

    cmake ../i-score -DCMAKE_MAKE_PROGRAM=/f/Qt/Tools/mingw491_32/bin/mingw32-make.exe -DCMAKE_C_COMPILER=/f/Qt/Tools/mingw491_32/bin/gcc.exe -DCMAKE_CXX_COMPILER=/f/Qt/Tools/mingw491_32/bin/g++.exe " -DCMAKE_PREFIX_PATH=/f/Qt/5.4/mingw491_32/lib/cmake/Qt5;/f/Qt/5.4/mingw491_32/lib/cmake/Qt5Test" -DBOOST_ROOT=/f/boost_1_57_0 -DISCORE_STATIC_PLUGINS:Bool=True -G"MSYS Makefiles" 
astyle command line : 

    astyle -A1 --indent=spaces=4 -C -xG -S -N -w -xw -f -U -k1 -W1 -j -xL --close-templates

cppcheck : 
    
    cppcheck -v --library=qt --xml --enable=all --quiet --std=c++11 -i base/plugins/device_explorer -I plugins/scenario -I lib -I lib/core -I ~/Qt/5.3/gcc_64/include/ base 2> cppcheck.xml 
    ./cppcheck-htmlreport --file=cppcheck.xml --report-dir=html   


# CMake Options

*/!\ OSX PATHS /!\*

## static, no deploy/bundle, no jamoma : 

    cmake ../../i-score -DCMAKE_PREFIX_PATH="/usr/local/Cellar/qt5/5.4.0/lib/cmake/Qt5;/usr/local/Cellar/qt5/5.4.0/lib/cmake/Qt5Test" -DISCORE_STATIC_PLUGINS:Bool=True 

## shared, no deploy/bundle, no jamoma : 

    cmake ../../i-score -DCMAKE_PREFIX_PATH="/usr/local/Cellar/qt5/5.4.0/lib/cmake/Qt5;/usr/local/Cellar/qt5/5.4.0/lib/cmake/Qt5Test" -DISCORE_STATIC_PLUGINS:Bool=False 

## shared, no deploy/bundle,  jamoma : 

    cmake ../../i-score -DCMAKE_PREFIX_PATH="/usr/local/jamoma/share/cmake/Jamoma;/usr/local/Cellar/qt5/5.4.0/lib/cmake/Qt5;/usr/local/Cellar/qt5/5.4.0/lib/cmake/Qt5Test" -DISCORE_STATIC_PLUGINS:Bool=False 

## static, deploy, jamoma :

    cmake ../../i-score -DCMAKE_PREFIX_PATH="/usr/local/jamoma/share/cmake/Jamoma;/usr/local/Cellar/qt5/5.4.0/lib/cmake/Qt5;/usr/local/Cellar/qt5/5.4.0/lib/cmake/Qt5Test" -DISCORE_STATIC_PLUGINS:Bool=True -DDEPLOYMENT_BUILD:Bool=True -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle   
    rm -rf bundle i-score.app
    cmake .
    make install

## Static, deploy, jamoma, kf5dnssd

    brew tap haraldf/kf5
    brew install kf5-kdnssd
    cmake ../i-score -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/usr/local/Cellar/kf5-kdnssd/5.9.0/lib/cmake/KF5DNSSD;/usr/local/jamoma/share/cmake/Jamoma;/usr/local/Cellar/qt5/5.4.1/lib/cmake/Qt5;/usr/local/Cellar/qt5/5.4.1/lib/cmake/Qt5Test" -DISCORE_BUILD_COHESION:Bool=True -DISCORE_STATIC_PLUGINS:Bool=True -DDEPLOYMENT_BUILD:Bool=True -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle
    make install
