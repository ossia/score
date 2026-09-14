set(CMAKE_MODULE_PATH
    "${CMAKE_MODULE_PATH}"
    "${SCORE_SDK}"
    "${SCORE_SDK}/lib"
    "${SCORE_SDK}/lib/cmake"
    "${SCORE_SDK}/lib/cmake/ossia"
    "${SCORE_SDK}/lib/cmake/score"
)

# Useful variables
set(SCORE_AVND_SOURCE_DIR "${SCORE_SDK}/lib/cmake/score")

# Add-ons call qt_add_resources, qt_wrap_cpp and friends unconditionally, and those
# come from Qt6CoreMacros, which only exists once find_package(Qt6) has run.
#
# Only the commands are wanted: score_lib_base does not link the Qt6:: targets. An
# add-on resolves Qt at load time against the score binary it is loaded into, and
# its include directories come from the SDK below, not from Qt's own targets.
list(APPEND CMAKE_PREFIX_PATH
    "${OSSIA_SDK}"
    "${OSSIA_SDK}/qt6-static"
    "${OSSIA_SDK}/qt6-static/lib64"
    "${OSSIA_SDK}/sysroot"
    "${OSSIA_SDK}/zlib"
)

find_package(Qt6 6.2 COMPONENTS Core
  OPTIONAL_COMPONENTS
    Gui
    Network
    Qml
    Quick
    ShaderTools
    Svg
    WebSockets
    Widgets
    Xml
)

if(NOT Qt6_FOUND)
  message(WARNING
    "Qt6 was not found in '${OSSIA_SDK}': add-ons that call qt_add_resources or "
    "any other qt_* command will fail to configure. Fetch the SDK with "
    "score/tools/fetch-sdk.sh.")
endif()

# Use the resource dir of the compiler actually in use, so that the builtin headers
# always match it (e.g. AppleClang vs the LLVM the SDK was built with); fall back to
# the headers shipped in the SDK.
execute_process(
  COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
  OUTPUT_VARIABLE CLANG_RESOURCE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT CLANG_RESOURCE_DIR OR NOT EXISTS "${CLANG_RESOURCE_DIR}")
  file(GLOB CLANG_RESOURCE_DIR "${SCORE_SDK}/lib/clang/*")
  list(GET CLANG_RESOURCE_DIR 0 CLANG_RESOURCE_DIR)
  string(STRIP "${CLANG_RESOURCE_DIR}" CLANG_RESOURCE_DIR)
endif()

# Find the Qt version
file(GLOB QTCORE_FILES LIST_DIRECTORIES true "${SCORE_SDK}/include/qt/QtCore/*")

foreach(dir ${QTCORE_FILES})
  if(IS_DIRECTORY "${dir}")
    cmake_path(GET dir FILENAME QT_INCLUDE_VERSION)
  else()
    continue()
  endif()
endforeach()

include(CheckCXXCompilerFlag)

# Add-ons gate on these. In-tree they come from libossia's OssiaConfiguration.cmake,
# which an SDK build never loads.
check_cxx_compiler_flag(-std=c++23 has_std_23_flag)
check_cxx_compiler_flag(-std=c++2b has_std_2b_flag)

# Create all the targets for the score plug-ins
foreach(_lib ${SCORE_PLUGINS})
  string(TOLOWER "${_lib}" _lib_lc)
  add_library(score_${_lib_lc} INTERFACE)
  target_compile_definitions(score_${_lib_lc} INTERFACE SCORE_${_lib})
endforeach()

# libc++ keeps __config_site in a directory named after the target triple, and
# <__config> includes it unconditionally. Find it instead of naming one triple:
# on aarch64 the directory is aarch64-unknown-linux-gnu, and hardcoding x86_64
# leaves every add-on failing on `'__config_site' file not found`.
file(GLOB _score_sdk_config_sites "${SCORE_SDK}/include/*/c++/v1/__config_site")
foreach(_score_sdk_config_site IN LISTS _score_sdk_config_sites)
  get_filename_component(_score_sdk_triple_inc "${_score_sdk_config_site}" DIRECTORY)
  target_compile_options(score_lib_base INTERFACE
    "SHELL:-Xclang -internal-isystem -Xclang ${_score_sdk_triple_inc}"
  )
endforeach()

set(CXX_VERSION_FLAG cxx_std_23)
set(CMAKE_CXX_STANDARD 23)

target_compile_options(score_lib_base INTERFACE
  -std=c++23
  -fPIC
)
target_compile_options(score_lib_base INTERFACE
  -nostdinc
  -nostdlib
  "SHELL:-Xclang -internal-isystem -Xclang ${SCORE_SDK}/include/c++/v1/"
  "SHELL:-Xclang -internal-isystem -Xclang ${SCORE_SDK}/include"
  "SHELL:-Xclang -internal-isystem -Xclang ${CLANG_RESOURCE_DIR}/include"
  "SHELL:-resource-dir ${CLANG_RESOURCE_DIR}"
)

if(APPLE)
  target_compile_options(score_lib_base INTERFACE
    -nostdinc++
  )
  target_include_directories(score_lib_base SYSTEM INTERFACE
    "${SCORE_SDK}/include/macos-sdks"
  )
endif()

# Only export plugin_instance
if(WIN32)
  target_link_libraries(score_lib_base INTERFACE
    "${SCORE_SDK}/lib/libscore.dll.a"
    ws2_32
  )
elseif(APPLE)
  target_link_libraries(score_lib_base INTERFACE
    -nostdlib++
    -Wl,-exported_symbol,_plugin_instance
  )

else()
  file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/retained-symbols.txt"
    CONTENT "plugin_instance"
  )
  target_link_libraries(score_lib_base INTERFACE
    -nostdlib++
    -Wl,--retain-symbols-file="${CMAKE_CURRENT_BINARY_DIR}/retained-symbols.txt"
  )
endif()

# The SDK ships no Qt CMake package, so the include layout is read off the headers
# it does ship.
set(SCORE_SDK_QT_INCLUDES
  "${SCORE_SDK}/include/score"
  "${SCORE_SDK}/include/qt")

file(GLOB _qt_module_dirs LIST_DIRECTORIES true "${SCORE_SDK}/include/qt/Qt*")
foreach(_qt_module_dir ${_qt_module_dirs})
  if(NOT IS_DIRECTORY "${_qt_module_dir}")
    continue()
  endif()
  cmake_path(GET _qt_module_dir FILENAME _qt_module)
  list(APPEND SCORE_SDK_QT_INCLUDES "${_qt_module_dir}")
  foreach(_sub
      "${_qt_module_dir}/${QT_INCLUDE_VERSION}"
      "${_qt_module_dir}/${QT_INCLUDE_VERSION}/${_qt_module}"
      "${_qt_module_dir}/${QT_INCLUDE_VERSION}/${_qt_module}/private")
    if(IS_DIRECTORY "${_sub}")
      list(APPEND SCORE_SDK_QT_INCLUDES "${_sub}")
    endif()
  endforeach()
endforeach()

target_include_directories(score_lib_base SYSTEM INTERFACE ${SCORE_SDK_QT_INCLUDES})

# For clang on non-Windows, qmetatype.h gives QMetaTypeForType and
# QMetaTypeInterfaceWrapper hidden visibility and externs them for every builtin
# type, on the assumption that each library emits its own copy. An add-on does not
# link Qt -- it resolves Qt from the score binary once dlopen'd -- and a hidden
# undefined symbol cannot be resolved that way, so declaring any Qt property fails
# to link. Emit that copy here, over exactly the type lists qmetatype.h externs.
# QMetaTypeForType has to come along: the wrapper's initialiser takes the addresses
# of its members, which `extern template class` suppresses too.
if(NOT WIN32 AND "${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/score_sdk_qt_metatypes.cpp" CONTENT [==[
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qbitarray.h>
#include <QtCore/qcborarray.h>
#include <QtCore/qcbormap.h>
#include <QtCore/qcborvalue.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qeasingcurve.h>
#include <QtCore/qfloat16.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qline.h>
#include <QtCore/qlocale.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qsize.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>
#include <QtCore/quuid.h>
#include <QtCore/qvariant.h>

QT_BEGIN_NAMESPACE
namespace QtPrivate
{
#define SCORE_SDK_DEFINE_METATYPE(TypeName, Id, Name) \
  template class QMetaTypeForType<Name>;              \
  template struct QMetaTypeInterfaceWrapper<Name>;

QT_FOR_EACH_STATIC_PRIMITIVE_NON_VOID_TYPE(SCORE_SDK_DEFINE_METATYPE)
QT_FOR_EACH_STATIC_PRIMITIVE_POINTER(SCORE_SDK_DEFINE_METATYPE)
QT_FOR_EACH_STATIC_CORE_CLASS(SCORE_SDK_DEFINE_METATYPE)
QT_FOR_EACH_STATIC_CORE_POINTER(SCORE_SDK_DEFINE_METATYPE)
QT_FOR_EACH_STATIC_CORE_TEMPLATE(SCORE_SDK_DEFINE_METATYPE)

#undef SCORE_SDK_DEFINE_METATYPE
}
QT_END_NAMESPACE
]==])
  target_sources(score_lib_base INTERFACE
    "${CMAKE_CURRENT_BINARY_DIR}/score_sdk_qt_metatypes.cpp")
endif()

target_compile_definitions(score_lib_base INTERFACE
  BOOST_MATH_DISABLE_FLOAT128=1
  BOOST_ASIO_DISABLE_CONCEPTS=1
  BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
  BOOST_MULTI_INDEX_ENABLE_SAFE_MODE

  QT_NO_LINKED_LIST
  QT_NO_JAVA_STYLE_ITERATORS
  QT_NO_USING_NAMESPACE
  QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
  QT_USE_QSTRINGBUILDER

  QT_CORE_LIB
  QT_GUI_LIB
  QT_NETWORK_LIB
  QT_NO_KEYWORDS
  QT_QML_LIB
  QT_QUICK_LIB
  QT_SERIALPORT_LIB
  QT_STATICPLUGIN
  QT_SVG_LIB
  QT_WEBSOCKETS_LIB
  QT_WIDGETS_LIB
  QT_XML_LIB

  RAPIDJSON_HAS_STDSTRING=1
  # SCORE_DEBUG
  TINYSPLINE_DOUBLE_PRECISION
  SCORE_DYNAMIC_PLUGINS=1
  QT_STATIC=1

  SCORE_LIB_BASE
  SCORE_LIB_DEVICE
  SCORE_LIB_INSPECTOR
  SCORE_LIB_LOCALTREE
  SCORE_LIB_PROCESS
  SCORE_LIB_STATE
  SCORE_PLUGIN_AUDIO
  SCORE_PLUGIN_AUTOMATION
  SCORE_PLUGIN_AVND
  SCORE_PLUGIN_CURVE
  SCORE_PLUGIN_DATAFLOW
  SCORE_PLUGIN_DEVICEEXPLORER
  SCORE_PLUGIN_ENGINE
  SCORE_PLUGIN_GFX
  SCORE_PLUGIN_LIBRARY
  SCORE_PLUGIN_MEDIA
  SCORE_PLUGIN_SCENARIO
  SCORE_PLUGIN_TRANSPORT
)

function(score_common_setup)
  enable_testing()
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_AUTOUIC OFF)
  set(CMAKE_AUTOMOC OFF)
endfunction()

function(setup_score_plugin PluginName)
  set_target_properties(${PluginName} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/"
  )

  if(APPLE)
    set_target_properties(${PluginName} PROPERTIES
      SUFFIX .dylib
    )
  endif()

  if(WIN32)
    install(TARGETS "${PluginName}"
      RUNTIME DESTINATION .
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION imports
    )
  else()
    install(TARGETS "${PluginName}"
      LIBRARY DESTINATION .
    )
  endif()
endfunction()

# The helpers below live in ScoreFunctions.cmake / ScoreTargetSetup.cmake for
# in-tree builds. Those cannot simply be included here: ScoreFunctions includes
# ScoreTargetSetup, which in turn includes six modules (Sanitize, UseGold,
# LinkerWarnings, DebugMode, ScoreStaticQt, GenerateStaticExport) that are not
# shipped in the SDK. Addons call them unconditionally, so mirror the
# self-contained ones here, as is already done for score_common_setup above.

function(score_write_file FileName Content)
  if(EXISTS "${FileName}")
    file(READ "${FileName}" EXISTING_CONTENT)
    string(REGEX REPLACE ";" "\\\\;" EXISTING_CONTENT "${EXISTING_CONTENT}")
    if(NOT "${Content}" STREQUAL "${EXISTING_CONTENT}")
      file(WRITE "${FileName}" ${Content})
    endif()
  else()
    file(WRITE "${FileName}" ${Content})
  endif()
endfunction()

function(score_generate_command_list_file TheTarget Headers)
    # Initialize our lists
    set(commandNameList)
    set(commandFileList)

    # First look for the SCORE_COMMAND_DECL(...) ones
    foreach(sourceFile ${Headers})
        file(READ "${sourceFile}" fileContent)
        string(REGEX MATCHALL "SCORE_COMMAND_DECL\\([A-Za-z_0-9\,\:<>\r\n\t ]*\\(\\)[A-Za-z_0-9\,\"'\:<>\+\r\n\t ]*\\)"
               defaultCommands "${fileContent}")

        foreach(fileLine ${defaultCommands})
            string(REPLACE "\n" "" fileLine "${fileLine}")
            string(REPLACE "\r" "" fileLine "${fileLine}")
            string(STRIP ${fileLine} strippedLine)

            string(REPLACE "," ";" lineAsList ${strippedLine})
            list(GET lineAsList 1 commandName)
            string(STRIP ${commandName} strippedCommandName)
            list(APPEND commandNameList "${strippedCommandName}")
        endforeach()

        # If there are matching strings, we add the file to our include list
        list(LENGTH defaultCommands matchingLines)
        if(${matchingLines} GREATER 0)
            list(APPEND commandFileList "${sourceFile}")
        endif()

        # Then look for the SCORE_COMMAND_DECL_T(...) ones
        string(REGEX MATCHALL "SCORE_COMMAND_DECL_T\\([A-Za-z_0-9\,\:<>\+\r\n\t ]*\\)"
               templateCommands "${fileContent}")
        foreach(fileLine ${templateCommands})
            string(REPLACE "\n" "" fileLine "${fileLine}")
            string(REPLACE "\r" "" fileLine "${fileLine}")
            string(STRIP ${fileLine} strippedLine)

            string(REPLACE "SCORE_COMMAND_DECL_T(" "" filtered1 ${strippedLine})
            string(REPLACE ")" "" commandName ${filtered1})
            string(STRIP ${commandName} strippedCommandName)

            list(APPEND commandNameList "${strippedCommandName}")
        endforeach()

        list(LENGTH templateCommands matchingLines)
        if(${matchingLines} GREATER 0)
            list(APPEND commandFileList "${sourceFile}")
        endif()

    endforeach()

    # Generate a file with the list of includes
    set(finalCommandFileList)
    foreach(sourceFile ${commandFileList})
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" strippedSourceFile ${sourceFile})
        set(finalCommandFileList "${finalCommandFileList}#include <${strippedSourceFile}>\n")
    endforeach()

    score_write_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${TheTarget}_commands_files.hpp"
        "${finalCommandFileList}"
        )

    # Generate a file with the list of types
    string(REPLACE ";" ", \n" commaSeparatedCommandList "${commandNameList}")
    score_write_file(
         "${CMAKE_CURRENT_BINARY_DIR}/${TheTarget}_commands.hpp"
         "${commaSeparatedCommandList}"
        )

endfunction()

function(score_optimize_in_debug_mode Target)
  if(NOT MSVC)
    if(NOT CMAKE_CROSSCOMPILING)
      if(CMAKE_BUILD_TYPE MATCHES ".*Debug.*")
        get_target_property(_has_custom_pch ${Target} SCORE_CUSTOM_PCH)
        if(NOT ${_has_custom_pch})
          set_target_properties(${Target} PROPERTIES SCORE_CUSTOM_PCH 1)
          target_compile_options(${Target} PRIVATE -Ofast ${SCORE_NATIVE_ARCH_FLAG})
        endif()
      endif()
    endif()
  endif()
endfunction()
