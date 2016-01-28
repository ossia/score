cmake_minimum_required(VERSION 3.0)
project(iscore_app LANGUAGES CXX)

enable_testing()
set(APPNAME "i-score-pnacl")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
iscore_write_static_plugins_header()

# Find dependencies
get_target_property(QtCore_LIB Qt5::Core LOCATION)
get_filename_component(QT_LIB_FOLDER ${QtCore_LIB} DIRECTORY)

find_library(qtharfbuzzng_LIBRARY NAMES qtharfbuzzng libqtharfbuzzng HINTS ${QT_LIB_FOLDER})
find_library(qtfreetype_LIBRARY NAMES qtfreetype libqtfreetype HINTS ${QT_LIB_FOLDER})
find_library(nacl_LIBRARY NAMES nacl libnacl)
find_library(ppapi_LIBRARY NAMES ppapi_gles2 libppapi_gles2)

# Executable
set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Application.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Application.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)

add_executable(${APPNAME} ${SRCS})

target_compile_options(${APPNAME} PUBLIC -Wl,--undefined=PSUserCreateInstance)
target_link_libraries(${APPNAME} PRIVATE
    iscore_lib_base
    ${ISCORE_PLUGINS_LIST}
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Svg Qt5::Xml
    m pthread

    /opt/nacl_sdk/pepper_42/toolchain/linux_pnacl/x86_64-nacl/lib/libppapi.a
    /opt/nacl_sdk/pepper_42/lib/pnacl/Release/libppapi_cpp.a
    /opt/nacl_sdk/pepper_42/lib/pnacl/Release/libppapi_cpp_private.a
    /opt/nacl_sdk/pepper_42/lib/pnacl/Release/libppapi_gles2.a
    /opt/nacl_sdk/pepper_42/lib/pnacl/Release/libnacl_io.a
    /opt/nacl_sdk/pepper_42/lib/pnacl/Release/libsdk_util.a

    /opt/QtNaCl_5.4/qtbase/lib/libqtharfbuzzng.a
)


setup_iscore_common_exe_features(${APPNAME})

install(TARGETS ${APPNAME}
  BUNDLE DESTINATION .
  RUNTIME DESTINATION bin)

include(IScoreDeployment)
