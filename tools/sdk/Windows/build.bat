# Install ActivePerl
# Install MinGW distro (Nuwen) 
# git clone https://code.qt.io/qt/qt5.git

cd qt5
git checkout 5.6.1
perl init-repository --module-subset=qtbase,qtimageformats,qtsvg,qtwebsockets,qttranslations,qtrepotools,qtdeclarative
cd ..

mkdir qt5-build
cd qt5-build

set PATH=C:\sdk-build\mingw64\bin;c:\Perl\bin;c:\Python27;%PATH%
set QMAKESPEC=win32-g++
c:/sdk-build/qt5/configure -release -prefix c:\sdk-build\qt5-release -opensource ^
                   -confirm-license ^
                   -nomake examples ^
                   -nomake tests ^
                   -no-qml-debug ^
                   -qt-zlib ^
                   -no-gif ^
                   -qt-libpng ^
                   -qt-libjpeg ^
                   -qt-pcre ^
                   -no-compile-examples ^
                   -no-cups ^
                   -no-iconv ^
                   -no-icu ^
                   -no-pch ^
                   -ltcg ^
                   -no-system-proxies ^
                   -opengl desktop
