# libisf

libisf is a library for parsing [Interactive Shader Format](https://www.interactiveshaderformat.com) shaders.

It comes with a visual editor to write and edit such shaders in real-time:

![isf editor](https://raw.githubusercontent.com/jcelerier/libisf/master/screenshot.png)

## Dependencies

### libisf

* C++17-conformant compiler (Xcode 10, VS 2017 15.8, GCC 7+, clang 6+)

### Editor

* CMake
* Qt 5
* [KTextEditor](https://api.kde.org/frameworks/ktexteditor/html/index.html) (optional, for a nice syntax highlighting)
* [QML Creative Controls](https://github.com/jcelerier/qml-creative-controls) (optional, to show UI controls for shaders)

## Building

    mkdir build
    cd build
    cmake /path/to/libisf
    make -j

## Running the editor

    ./editor
