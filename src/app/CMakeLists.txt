project(score_app LANGUAGES CXX)

score_common_setup()
score_write_static_plugins_header()
include(CheckSymbolExists)
enable_testing()
set(APPNAME "score")

# PIE / PIC has some issues with PCH...
set(CMAKE_POSITION_INDEPENDENT_CODE 1)
if(SCORE_PCH)
  if(UNIX AND NOT APPLE)
    add_compile_options(-fPIC)
  endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES Emscripten)
  add_link_options("SHELL:-sASSERTIONS=0")
  add_link_options("SHELL:-sSAFE_HEAP=0")
  add_link_options("SHELL:-sUSE_SDL=2")
  add_link_options("SHELL:-sSTACK_OVERFLOW_CHECK=0")
  #add_link_options("SHELL:--source-map-base http://127.0.0.1:8000/")
  add_link_options("SHELL:-sFETCH=1")
  add_link_options("SHELL:-sWASM=1")
  add_link_options("SHELL:-sFULL_ES2=1")
  add_link_options("SHELL:-sFULL_ES3=1")
  add_link_options("SHELL:-sUSE_WEBGL2=1")
  #add_link_options("SHELL:-s EXIT_RUNTIME=1")
  add_link_options("SHELL:-sERROR_ON_UNDEFINED_SYMBOLS=1")
  #add_link_options("SHELL:-s PTHREAD_POOL_SIZE=16")
  add_link_options("SHELL:-sTOTAL_MEMORY=2000MB")
  #add_link_options("SHELL:-s ALLOW_MEMORY_GROWTH=1")
  #add_link_options("SHELL:-s MAXIMUM_MEMORY=4GB")

  ## Qt5
  #  #add_link_options("SHELL:-s USE_PTHREADS=1")
  #  add_link_options("SHELL:-s EXTRA_EXPORTED_RUNTIME_METHODS=[\"UTF16ToString\",\"stringToUTF16\"]")

  ## Qt 6
  add_link_options("SHELL:-sUSE_PTHREADS=1")
  # add_link_options("SHELL:-s FETCH=1")
  add_link_options("SHELL:-sWASM_BIGINT=1")
  add_link_options("SHELL:-sMODULARIZE=1")
  add_link_options("SHELL:-sMAX_WEBGL_VERSION=2")
  add_link_options("SHELL:-sWEBGL2_BACKWARDS_COMPATIBILITY_EMULATION=1")
  # add_link_options("SHELL:-pthread")
  add_link_options("SHELL:-sASYNCIFY" "-s" "ASYNCIFY_IMPORTS=[qt_asyncify_suspend_js, qt_asyncify_resume_js]")

  add_link_options("SHELL:-sABORTING_MALLOC=1")
  # add_link_options("SHELL:-sJS_MATH=1")
  add_link_options("SHELL:-sSTACK_SIZE=8MB")
  add_link_options("SHELL:-sPTHREAD_POOL_SIZE=16")
  add_link_options("--bind")

  # set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Application.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Application.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"

  "${3RDPARTY_FOLDER}/phantomstyle/src/phantom/phantomcolor.cpp"
  "${3RDPARTY_FOLDER}/phantomstyle/src/phantom/phantomstyle.cpp"
)

if(EMSCRIPTEN)
  qt_add_executable(${APPNAME} MANUAL_FINALIZATION ${SRCS})
  set_target_properties(${APPNAME} PROPERTIES QT_WASM_EXTRA_EXPORTED_METHODS "UTF16ToString,stringToUTF16,JSEvents,specialHTMLTargets,FS,callMain,ENV") 
else()
  add_executable(score WIN32 ${SRCS})
endif()

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
endif()

if(TARGET snmalloc)
  target_link_libraries(${APPNAME} PUBLIC snmalloc)
  if(NOT WIN32)
  #  target_link_libraries(${APPNAME} PUBLIC snmallocshim-static)
  endif()
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

find_path(FFTW_INCLUDE_DIR fftw3.h)
if(FFTW_INCLUDE_DIR)
  find_library(FFTW_LIBRARY fftw3)
  find_library(FFTW_THREADS_LIBRARY fftw3_threads)
  if(FFTW_THREADS_LIBRARY)
    set(FFTW_HAS_THREADS 1)
    target_include_directories(${APPNAME} PRIVATE ${FFTW_INCLUDE_DIR})
    target_link_libraries(${APPNAME} PRIVATE ${FFTW_THREADS_LIBRARY})
  elseif(FFTW_LIBRARY)
    if(MSVC)
      set(FFTW_HAS_THREADS 1)
    else()
      check_symbol_exists(fftw_make_planner_thread_safe ${FFTW_LIBRARY} FFTW_HAS_THREADS)
    endif()

    if(FFTW_HAS_THREADS)
      target_include_directories(${APPNAME} PRIVATE ${FFTW_INCLUDE_DIR})
      target_link_libraries(${APPNAME} PRIVATE ${FFTW_LIBRARY})
    endif()
  endif()
endif()
if(FFTW_HAS_THREADS)
  target_compile_definitions(${APPNAME} PRIVATE FFTW_HAS_THREADS=1)
endif()
setup_score_common_exe_features(${APPNAME})

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
    RUNTIME DESTINATION "${SCORE_BIN_INSTALL_DIR}"
    COMPONENT OssiaScore)
  install(
    TARGETS ${APPNAME}
    ARCHIVE DESTINATION lib
    COMPONENT Devel)
elseif(APPLE)
  # done in ScoreDeploymentOSX
else() # Linux
  install(
    TARGETS ${APPNAME}
    RUNTIME DESTINATION bin
    COMPONENT OssiaScore)
endif()

include(ScoreDeployment)

if(EMSCRIPTEN)
  qt_finalize_executable(${APPNAME})
else()  
  disable_qt_plugins(${APPNAME})
  enable_minimal_qt_plugins(${APPNAME} 1)
endif()
