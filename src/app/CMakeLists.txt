project(score_app LANGUAGES CXX)

score_common_setup()
score_write_static_plugins_header()
include(CheckSymbolExists)
enable_testing()
set(APPNAME "score")
set(CMAKE_POSITION_INDEPENDENT_CODE 1)

if(CMAKE_SYSTEM_NAME MATCHES Emscripten)
  add_link_options("SHELL:-s ASSERTIONS=0")
  add_link_options("SHELL:-s SAFE_HEAP=0")
  add_link_options("SHELL:-s USE_SDL=2")
  add_link_options("SHELL:-s STACK_OVERFLOW_CHECK=0")
  #add_link_options("SHELL:--source-map-base http://127.0.0.1:8000/")
  add_link_options("SHELL:-s FETCH=1")
  add_link_options("SHELL:-s WASM=1")
  add_link_options("SHELL:-s FULL_ES2=1")
  add_link_options("SHELL:-s FULL_ES3=1")
  add_link_options("SHELL:-s USE_WEBGL2=1")
  #add_link_options("SHELL:-s EXIT_RUNTIME=1")
  add_link_options("SHELL:-s ERROR_ON_UNDEFINED_SYMBOLS=1")
  #add_link_options("SHELL:-s PTHREAD_POOL_SIZE=16")
  add_link_options("SHELL:-s TOTAL_MEMORY=2000MB")
  #add_link_options("SHELL:-s ALLOW_MEMORY_GROWTH=1")
  #add_link_options("SHELL:-s MAXIMUM_MEMORY=4GB")

  ## Qt5
  #  #add_link_options("SHELL:-s USE_PTHREADS=1")
  #  add_link_options("SHELL:-s EXTRA_EXPORTED_RUNTIME_METHODS=[\"UTF16ToString\",\"stringToUTF16\"]")

  ## Qt 6
  add_link_options("SHELL:-s USE_PTHREADS=1")
  # add_link_options("SHELL:-s FETCH=1")
  add_link_options("SHELL:-s WASM_BIGINT=1")
  add_link_options("SHELL:-s MODULARIZE=1")
  add_link_options("SHELL:-s MAX_WEBGL_VERSION=2")
  add_link_options("SHELL:-s WEBGL2_BACKWARDS_COMPATIBILITY_EMULATION=1")
  # add_link_options("SHELL:-pthread")
  add_link_options("SHELL:-s ASYNCIFY" "-s" "ASYNCIFY_IMPORTS=[qt_asyncify_suspend_js, qt_asyncify_resume_js]")

  add_link_options("--bind")

  set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

#add_executable(score-main WIN32 AppLauncher.cpp)
add_executable(${APPNAME} WIN32
  "${CMAKE_CURRENT_SOURCE_DIR}/Application.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Application.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"

  "${3RDPARTY_FOLDER}/phantomstyle/src/phantom/phantomcolor.cpp"
  "${3RDPARTY_FOLDER}/phantomstyle/src/phantom/phantomstyle.cpp"

)
#target_link_libraries(score-main PRIVATE ${APPNAME})

ossia_set_visibility(${APPNAME})

target_include_directories(${APPNAME} PRIVATE "${3RDPARTY_FOLDER}/phantomstyle/src")

target_link_libraries(${APPNAME}
  PUBLIC
    score_lib_base
    ${QT_PREFIX}::CorePrivate
)

if(SCORE_STATIC_PLUGINS)
  target_link_libraries(${APPNAME} PUBLIC ${SCORE_PLUGINS_LIST})
  set(SCORE_MIMALLOC_TARGET mimalloc-static)
else()
  set(SCORE_MIMALLOC_TARGET mimalloc)
endif()

if(NOT asan IN_LIST CNINJA_FEATURES)
#  disable mimalloc until we find out why it causes jack to be stuck..;
#  if(NOT CMAKE_SYSTEM_NAME MATCHES Emscripten)
#    target_link_libraries(${APPNAME} PUBLIC
#      $<LINK_ONLY:${SCORE_MIMALLOC_TARGET}>
#    )
#  endif()
endif()

target_compile_definitions(${APPNAME} PUBLIC
    $<$<CONFIG:Debug>:SCORE_SOURCE_DIR="${SCORE_ROOT_SOURCE_DIR}">
)
if(TARGET score_plugin_jit OR TARGET score_plugin_faust)
  set_target_properties(${APPNAME} PROPERTIES ENABLE_EXPORTS 1)
  if(MINGW OR MSYS)
    target_link_libraries(${APPNAME} PUBLIC -Wl,--stack,16777216)
    # target_link_libraries(score-main PUBLIC -Wl,--stack,16777216)
  endif()
endif()

if(MSVC)
  # Necessary for JACK on windows.
  # http://jack-audio.10948.n7.nabble.com/Cannot-run-64bit-windows-client-td18952.html
  set_property(
    TARGET ${APPNAME}
    APPEND PROPERTY
      LINK_FLAGS "/DYNAMICBASE:NO")
endif()

if(WIN32)
  if(MSVC OR ("${CMAKE_RC_COMPILER}" MATCHES ".*llvm.*"))
    target_sources(${APPNAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/score.rc")
  else()
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/score-icon.obj"
                       COMMAND "${CMAKE_RC_COMPILER}"
                            -D GCC_WINDRES
                            -I "${CMAKE_CURRENT_SOURCE_DIR}"
                            -I "${CMAKE_CURRENT_BINARY_DIR}"
                            -o "${CMAKE_CURRENT_BINARY_DIR}/score-icon.obj"
                            -i "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/score.rc"
                        VERBATIM)
    target_sources(${APPNAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/score-icon.obj")
  endif()
endif()

if(UNIX AND NOT APPLE)
  # Linux package is called ossia-score
  set_target_properties(${APPNAME}
    PROPERTIES
      OUTPUT_NAME ossia-score)

  target_link_libraries(${APPNAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

if(SCORE_LINKER_SCRIPT)
  target_link_libraries(${APPNAME} PRIVATE "-Wl,--version-script,${SCORE_LINKER_SCRIPT}")
endif()

if(APPLE)
    find_library(CFLIB CoreFoundation)
    find_library(FLIB Foundation)
    target_sources(${APPNAME} PRIVATE mac_main.m)
    target_link_libraries(${APPNAME} PRIVATE ${CFLIB} ${FLIB})

    if(SCORE_DEPLOYMENT_BUILD)
      set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/score.icns")
      set_source_files_properties(${ICON_SRC} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

      target_sources(${APPNAME} PRIVATE ${ICON_SRC})
      set_target_properties(${APPNAME} PROPERTIES MACOSX_BUNDLE TRUE)
    endif()
endif()

if(WIN32)
  target_link_libraries(${APPNAME} PRIVATE ntdll)
endif()
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

  get_target_property(QtCore_LIB ${QT_PREFIX}::Core LOCATION)
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

find_path(FFTW3_INCLUDEDIR fftw3.h)
if(FFTW3_INCLUDEDIR)
  find_library(FFTW3_LIBRARY fftw3)
  find_library(FFTW3_THREADS_LIBRARY fftw3_threads)
  if(FFTW3_THREADS_LIBRARY)
    set(FFTW3_HAS_THREADS 1)
    target_include_directories(${APPNAME} PRIVATE ${FFTW3_INCLUDEDIR})
    target_link_libraries(${APPNAME} PRIVATE ${FFTW3_THREADS_LIBRARY})
  elseif(FFTW3_LIBRARY)
    if(MSVC)
      set(FFTW3_HAS_THREADS 1)
    else()
      check_symbol_exists(fftw_make_planner_thread_safe ${FFTW3_LIBRARY} FFTW3_HAS_THREADS)
    endif()

    if(FFTW3_HAS_THREADS)
      target_include_directories(${APPNAME} PRIVATE ${FFTW3_INCLUDEDIR})
      target_link_libraries(${APPNAME} PRIVATE ${FFTW3_LIBRARY})
    endif()
  endif()
endif()
if(FFTW3_HAS_THREADS)
  target_compile_definitions(${APPNAME} PRIVATE FFTW3_HAS_THREADS=1)
endif()
setup_score_common_exe_features(${APPNAME})

if(WIN32)
    file(GENERATE
        OUTPUT "${CMAKE_BINARY_DIR}/qt.conf"
        CONTENT  "[Platforms]
WindowsArguments = fontengine=freetype
"
         )
endif()

if(UNIX AND NOT APPLE)
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/run.sh")
      get_target_property(QtCore_LIB ${QT_PREFIX}::Core LOCATION)
      get_filename_component(QT_ROOT_FOLDER "${QtCore_LIB}" DIRECTORY)
      get_filename_component(QT_ROOT_FOLDER "${QT_ROOT_FOLDER}" ABSOLUTE)
      file(
        WRITE "${CMAKE_CURRENT_BINARY_DIR}/run.sh"
        "#!/bin/sh\nLD_LIBRARY_PATH=${QT_ROOT_FOLDER} ./ossia-score $@\n"
      )

      file(
        COPY "${CMAKE_CURRENT_BINARY_DIR}/run.sh"
        DESTINATION "${CMAKE_BINARY_DIR}"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
      )
  endif()
endif()

if(WIN32)
  install(
    TARGETS ${APPNAME}
    RUNTIME DESTINATION .
    COMPONENT OssiaScore)
elseif(APPLE)
  install(
    TARGETS ${APPNAME}
    BUNDLE DESTINATION .
    COMPONENT OssiaScore)
else() # Linux
  install(
    TARGETS ${APPNAME}
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()

include(ScoreDeployment)

disable_qt_plugins(${APPNAME})
enable_minimal_qt_plugins(${APPNAME})
