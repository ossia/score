# Qt Android CMake utility

## What it is

When using Qt for Android development, QMake & QtCreator is the only sane option for compiling and deploying. But if you prefer CMake, you're stuck and have no choice but writing .pro files that duplicate the functionality of your CMake files.

This utility tries to avoids this by providing a CMake way of doing Android compilation and deployment, without QtCreator. It is based on:

* the Android CMake toolchain at https://github.com/taka-no-me/android-cmake
* the ```androiddeployqt``` utility from the Qt Android SDK
* the QML / Android example at https://github.com/calincru/QML-Android-Demo

This utility has been developed for my own needs. Don't hesitate to use / share / fork / modify / improve it freely :)

## How to use it

### How to integrate it to your CMake configuration

The toolchain file defines the ```ANDROID``` variable, so that everything which is added specifically for Android in your CMake files can be surrounded with

```cmake
if(ANDROID)
    ...
endif()
```

The first thing to do is to change your executable target into a library, because on Android, the entry point has to be a Java activity, and your C++ code is then loaded (as a library) and called by this activity.

```cmake
if(ANDROID)
    add_library(my_app SHARED ...)
else()
    add_executable(my_app ...)
endif()
```

Then all you have to do is to call the ```add_qt_android_apk``` macro to create a new target that will create the Android APK.

```cmake
if(ANDROID)
    include(qt-android-cmake/AddQtAndroidApk.cmake)
    add_qt_android_apk(my_app_apk my_app)
endif()
```

And that's it. Your APK can now be created by running "make" (or "cmake --build ." if you don't want to bother typing the full path to the make.exe program included in the NDK).

Of course, ```add_qt_android_apk``` accepts more options, see below for the detail.

### How to run CMake

First, you must make sure that the following environment variables are defined:

* ```ANDROID_NDK```: root directory of the Android NDK
* ```JAVA_HOME```: root directory of the Java JDK

**IMPORTANT** ```JAVA_HOME``` must be defined when you compile the APK too.

Additionally you can define the following ones, but you can also define them as CMake variables if you prefer:

* ```ANDROID_SDK```: root directory of the Android SDK
* ```ANT```: Path of the ANT executable

You can then run CMake:.

**On Windows**
```
cmake -G"MinGW Makefiles"
      -DCMAKE_TOOLCHAIN_FILE=<qt-android-cmake>/toolchain/android.toolchain.cmake 
      -DCMAKE_MAKE_PROGRAM="%ANDROID_NDK%/prebuilt/windows/bin/make.exe" .
```

**On Linux**
```
cmake -DCMAKE_TOOLCHAIN_FILE=path/to/the/android.toolchain.cmake .
```

**On Mac OS X**
```
This utility has not been tested on this OS yet :)
```

The Android toolchain can be customized with environment variables and/or CMake variables. Refer to its documentation (at the beginning of the toolchain file) for more details.

## Options of the ```add_qt_android_apk``` macro

The first two arguments of the macro are the name of the APK target to be created, and the target it must be based on (your executable). These are of course mandatory.

The macro also accepts optional named arguments. Any combination of these arguments is valid, so that you can customize the generated APK according to your own needs.

Here is the full list of possible arguments:

### NAME

The name of the application. If not given, the name of the source target is taken.

Example:

```cmake
add_qt_android_apk(my_app_apk my_app
    NAME "My App"
)
```

### PACKAGE_NAME

The name of the application package. If not given, "org.qtproject.${source_target}" , where source_target is the name of the source target, is taken.

Example:

```cmake
add_qt_android_apk(my_app_apk my_app
    PACKAGE_NAME "org.mycompany.myapp"
)
```

### PACKAGE_SOURCES

The path to a directory containing additional files for the package (custom manifest, resources, translations, Java classes, ...). If you were using a regular QMake project file (.pro), this directory would be the one that you assign to the  ```ANDROID_PACKAGE_SOURCE_DIR``` variable.

If you don't provide this argument, a default manifest is generated from the ```AndroidManifest.xml.in``` template and automatically used for building the APK.

Example:

```cmake
add_qt_android_apk(my_app_apk my_app
    PACKAGE_SOURCES ${CMAKE_CURRENT_LIST_DIR}/my-android-sources
)
```

### KEYSTORE

The path to a keystore file and an alias, for signing the APK. If not provided, the APK won't be signed.

Example:

```cmake
add_qt_android_apk(my_app_apk my_app
    KEYSTORE ${CMAKE_CURRENT_LIST_DIR}/mykey.keystore myalias
)
```

### KEYSTORE_PASSWORD

The password associated to the gicen keystore. Note that this option is only considered if the ```KEYSTORE``` argument is used. If it is not given, the password will be asked directly in the console at build time.

Example:

```cmake
add_qt_android_apk(my_app_apk my_app
    KEYSTORE ${CMAKE_CURRENT_LIST_DIR}/mykey.keystore myalias
    KEYSTORE_PASSWORD xxxxx
)
```

### DEPENDS

A list of dependencies (libraries) to be included into the APK. All the dependencies of the application must be listed here; if one is missing, the deployed application will fail to run on the device. The listed items can be either target names, or library paths.

Example:

```cmake
add_qt_android_apk(my_app_apk my_app
    DEPENDS a_linked_target "path/to/a_linked_library.so" etc.
)
```

### INSTALL

If this option is given, the created APK will be deployed to a connected Android device. By default, the chosen device is the default one, i.e. the first one of the ADB device list.

Example:

```cmake
add_qt_android_apk(my_app_apk my_app
    INSTALL
)
```

## Contact

Laurent Gomila: laurent.gom@gmail.com
