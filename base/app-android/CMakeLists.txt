cmake_minimum_required(VERSION 3.0)
 
project(iscore_app_android)

find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/base/app-android/libs/${ARM_TARGET}/)
SET(CMAKE_SKIP_BUILD_RPATH FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
SET(CMAKE_INSTALL_RPATH ".")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(ANDROID_ABI "armeabi")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib/core")

include_directories("${ANDROID_NDK}/platforms/android-19/arch-arm/usr/include/")
include_directories("${ANDROID_NDK}/sources/android/native_app_glue")
 
add_library(iscore_app_android SHARED 
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    main.cpp)
 
target_link_libraries(iscore_app_android iscore_lib)
target_link_libraries(iscore_app_android Qt5::Core Qt5::Widgets)
target_link_libraries(iscore_app_android log "${ANDROID_NDK}/platforms/android-19/arch-arm/usr/lib/libandroid.so")

 
# TODO check if there is a better macro.
set(ANDROID_DEPLOY_QT "${_qt5_install_prefix}/../../bin/androiddeployqt")

# Look for androiddeployqt program
if (NOT ANDROID_DEPLOY_QT)
  message(FATAL_ERROR "Could not find androiddeployqt. Make sure you set all the Qt5_* paths to the Qt for android compiled version.")
endif(NOT ANDROID_DEPLOY_QT)

# Set version
set(ANDROID_VERSION_NAME "03")

# set android package source for androiddeployqt json file
set(PACKAGE_SOURCE_ANDROID ${CMAKE_BINARY_DIR}/tmpAndroid)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/AndroidManifest.xml.cmake ${PACKAGE_SOURCE_ANDROID}/AndroidManifest.xml @ONLY)

# create json file parsed by the androiddeployqt
set(ANDROID_SDK $ENV{ANDROID_SDK})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configAndroid.json.cmake ${PACKAGE_SOURCE_ANDROID}/configAndroid.json @ONLY)

add_custom_command (
  OUTPUT createApkFromAndroidDeployQtDebug
  DEPENDS ${PACKAGE_SOURCE_ANDROID}/AndroidManifest.xml
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR} ${PACKAGE_SOURCE_ANDROID}
  COMMAND ${ANDROID_DEPLOY_QT} --output ${CMAKE_CURRENT_BINARY_DIR}/ --input ${PACKAGE_SOURCE_ANDROID}/configAndroid.json --debug
  COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/bin/QtApp-debug.apk ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-debug-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk
)

add_custom_command (
  OUTPUT createApkFromAndroidDeployQtRelease
  DEPENDS ${PACKAGE_SOURCE_ANDROID}/AndroidManifest.xml
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR} ${PACKAGE_SOURCE_ANDROID}
  COMMAND ${ANDROID_DEPLOY_QT} --output ${CMAKE_CURRENT_BINARY_DIR}/ --input ${PACKAGE_SOURCE_ANDROID}/configAndroid.json --release
  COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/bin/QtApp-release-unsigned.apk ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk
)

add_custom_command (
  OUTPUT createApkSignedFromAndroidDeployQtRelease
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-signed-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk
  COMMAND jarsigner -verbose -sigalg SHA1withRSA -digestalg SHA1 -keystore $ENV{ANDROID_KEYSTORE} -storepass $ENV{ANDROID_KEYSTORE_PASSWD} ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-signed-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk iscore
)

add_custom_command (
  OUTPUT createApkSignedAlignedFromAndroidDeployQtRelease
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-signed-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-signed-aligned-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk
  COMMAND ${ANDROID_SDK}/build-tools/20.0.0/zipalign -v 4 ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-signed-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-signed-aligned-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk
)

add_custom_command (
  OUTPUT apkInstall
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-signed-aligned-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk
  COMMAND ${ANDROID_SDK}/platform-tools/adb install -r ${CMAKE_CURRENT_BINARY_DIR}/bin/iscore-Android-release-signed-aligned-${ANDROID_ABI}-${ANDROID_VERSION_NAME}.apk
)

# Command to create apk from Makefile
add_custom_target (apk_debug
   DEPENDS createApkFromAndroidDeployQtDebug
)

# Command to create apk from Makefile
add_custom_target (apk_release
   DEPENDS createApkFromAndroidDeployQtRelease
)

# Command to create signed apk from Makefile
add_custom_target (apk_signed
   DEPENDS createApkSignedFromAndroidDeployQtRelease
)

# Command to create signed aligned apk from Makefile
add_custom_target (apk_signed_aligned
   DEPENDS createApkSignedAlignedFromAndroidDeployQtRelease
)

# Command to install the signed aligned apk through adb from Makefile
add_custom_target (apk_install
   DEPENDS apkInstall
)
