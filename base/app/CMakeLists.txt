cmake_minimum_required(VERSION 3.0)
project(iscore_app LANGUAGES CXX)

iscore_common_setup()
enable_testing()
set(APPNAME "i-score")
# TODO strip symbols after deployment build
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Application.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Application.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)
if(WIN32)
set(SRCS ${SRCS} "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/i-score.rc")
endif()

if(APPLE)
  set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/i-score.icns")
  set_source_files_properties(${ICON_SRC} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

if(DEPLOYMENT_BUILD)
    add_executable(${APPNAME} WIN32 MACOSX_BUNDLE ${SRCS} ${ICON_SRC})
else()
    add_executable(${APPNAME} WIN32 ${SRCS})
endif()


target_link_libraries(${APPNAME} PUBLIC iscore_lib_base)

if(ISCORE_STATIC_PLUGINS)
  iscore_write_static_plugins_header()
  target_link_libraries(${APPNAME} PUBLIC ${ISCORE_PLUGINS_LIST})
endif()

include(StaticApp.cmake)

if(IOS)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(COREAUDIO_LIBRARY CoreAudio)
  find_library(CORETEXT_LIBRARY CoreText)
  find_library(FOUNDATION_LIBRARY Foundation)
  find_library(ACCELERATE_LIBRARY Accelerate)
  find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
  find_library(SECURITY_LIBRARY Security)
  find_library(MobileCoreServices_LIBRARY MobileCoreServices)
  find_library(SystemConfiguration_LIBRARY SystemConfiguration)
  find_library(CoreGraphics_LIBRARY CoreGraphics)
  find_library(UIKit_LIBRARY UIKit)

  get_target_property(QtCore_LIB Qt5::Core LOCATION)
  get_filename_component(QT_LIB_FOLDER ${QtCore_LIB} DIRECTORY)
  find_library(qtharfbuzzng_LIBRARY NAMES qtharfbuzzng libqtharfbuzzng HINTS ${QT_LIB_FOLDER})
  find_library(qtfreetype_LIBRARY NAMES qtfreetype libqtfreetype HINTS ${QT_LIB_FOLDER})
  find_library(qtpcre_LIBRARY NAMES qtpcre libqtpcre HINTS ${QT_LIB_FOLDER})
  target_link_libraries(${APPNAME} PUBLIC ${qtharfbuzzng_LIBRARY} ${qtfreetype_LIBRARY} ${qtpcre_LIBRARY} 
    ${CoreGraphics_LIBRARY} ${CORETEXT_LIBRARY} ${COREFOUNDATION_LIBRARY} 
    ${COREAUDIO_LIBRARY} ${SECURITY_LIBRARY} ${FOUNDATION_LIBRARY} 
    ${ACCELERATE_LIBRARY} ${AUDIOTOOLBOX_LIBRARY}
    ${MobileCoreServices_LIBRARY} ${SystemConfiguration_LIBRARY} ${UIKit_LIBRARY}
    )
  
endif()

setup_iscore_common_exe_features(${APPNAME})

install(
  TARGETS ${APPNAME}
  BUNDLE DESTINATION .
  RUNTIME DESTINATION bin)

include(UnityApp.cmake)
include(IScoreDeployment)
