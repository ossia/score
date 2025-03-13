# OSSIA-specific configuration
set(OSSIA_PCH "${SCORE_PCH}" CACHE INTERNAL "" FORCE)
set(OSSIA_STATIC "${SCORE_STATIC_PLUGINS}" CACHE INTERNAL "" FORCE)

if(SCORE_ENABLE_CXX26)
  set(OSSIA_CXX_STANDARD "26" CACHE INTERNAL "" FORCE)
elseif(SCORE_ENABLE_CXX23)
  set(OSSIA_CXX_STANDARD "23" CACHE INTERNAL "" FORCE)
else()
  set(OSSIA_CXX_STANDARD "20" CACHE INTERNAL "" FORCE)
endif()

set(OSSIA_CALLBACK_CONTAINER_MUTEX "std::mutex" CACHE INTERNAL "" FORCE)

if(NOT SCORE_INSTALL_HEADERS)
  set(OSSIA_NO_INSTALL ON CACHE INTERNAL "" FORCE)
endif()
set(OSSIA_MATH_EXPRESSION ON CACHE INTERNAL "" FORCE)

if(SCORE_USE_SYSTEM_LIBRARIES)
  set(OSSIA_USE_SYSTEM_LIBRARIES ON CACHE INTERNAL "" FORCE)
endif()

if(UNIX AND NOT APPLE AND SCORE_DYNAMIC_PLUGINS)
  set(OSSIA_FAST_DEVELOPER_BUILD ON CACHE INTERNAL "" FORCE)
endif()

# FFT-relatd features
set(OSSIA_ENABLE_FFT ON CACHE INTERNAL "" FORCE)

if(NOT DEFINED OSSIA_ENABLE_FFTW AND NOT DEFINED OSSIA_ENABLE_KFR)
  if(SCORE_USE_SYSTEM_LIBRARIES)
    set(OSSIA_ENABLE_FFTW ON CACHE INTERNAL "" FORCE)
    set(OSSIA_ENABLE_KFR OFF CACHE INTERNAL "" FORCE)
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Emscripten")
    set(OSSIA_ENABLE_KFR ON CACHE INTERNAL "" FORCE)
    set(OSSIA_ENABLE_FFTW OFF CACHE INTERNAL "" FORCE)
  else()
    set(OSSIA_ENABLE_FFTW ON CACHE INTERNAL "" FORCE)
    set(OSSIA_ENABLE_KFR OFF CACHE INTERNAL "" FORCE)
  endif()
endif()

# If KFR is not specifically disabled, we enable it
if(NOT DEFINED OSSIA_ENABLE_KFR)
  set(OSSIA_ENABLE_KFR ON CACHE INTERNAL "" FORCE)
endif()

if(SCORE_DEPLOYMENT_BUILD)
  if(EMSCRIPTEN)
    set(KFR_ENABLE_DFT_MULTIARCH 0)
    set(KFR_ENABLE_DFT_MULTIARCH 0 CACHE "" INTERNAL)
  else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES x86)
      set(KFR_ENABLE_DFT_MULTIARCH 1)
      set(KFR_ENABLE_DFT_MULTIARCH 1 CACHE "" INTERNAL)
    else()
      set(KFR_ENABLE_DFT_MULTIARCH 0)
      set(KFR_ENABLE_DFT_MULTIARCH 0 CACHE "" INTERNAL)
    endif()
  endif()
endif()

if(NOT CPU_ARCH AND NOT KFR_ARCH)
  if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES x86)
      set(KFR_ARCH avx)
      set(KFR_ARCH avx CACHE "" INTERNAL)
    endif()
  else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES x86)
      set(KFR_ARCH sse2)
      set(KFR_ARCH sse2 CACHE "" INTERNAL)
    endif()
  endif()
endif()

set(OSSIA_PD OFF CACHE INTERNAL "" FORCE)
set(OSSIA_MAX OFF CACHE INTERNAL "" FORCE)
set(OSSIA_PYTHON OFF CACHE INTERNAL "" FORCE)
set(OSSIA_UNITY3D OFF CACHE INTERNAL "" FORCE)
set(OSSIA_JAVA OFF CACHE INTERNAL "" FORCE)
set(OSSIA_OSX_FAT_LIBRARIES OFF CACHE INTERNAL "" FORCE)
set(OSSIA_PYTHON OFF CACHE INTERNAL "" FORCE)
set(OSSIA_QT ON CACHE INTERNAL "" FORCE)
set(OSSIA_QT_REQUIRED "REQUIRED" CACHE INTERNAL "" FORCE)
set(OSSIA_QML_DEVICE OFF CACHE INTERNAL "" FORCE)
set(OSSIA_DISABLE_QT_PLUGIN ON CACHE INTERNAL "" FORCE)
set(OSSIA_HIDE_ALL_SYMBOLS ON CACHE INTERNAL "" FORCE)
set(OSSIA_NO_DLLIMPORT ON CACHE INTERNAL "" FORCE)

set(OSSIA_PROTOCOL_MIDI ON CACHE INTERNAL "" FORCE)
if(SCORE_DISABLE_PROTOCOLS)
  set(OSSIA_PROTOCOL_HTTP OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_WEBSOCKETS OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_SERIAL OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_ARTNET OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_JOYSTICK OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_WIIMOTE OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_MINUIT OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_OSCQUERY OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_MQTT5 OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_COAP OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_PARALLEL OFF CACHE INTERNAL "" FORCE)
  set(OSSIA_DNSSD OFF CACHE INTERNAL "" FORCE)
else()
  set(OSSIA_PROTOCOL_JOYSTICK ON CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_SERIAL ON CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_HTTP ON CACHE INTERNAL "" FORCE)
  set(OSSIA_PROTOCOL_WEBSOCKETS ON CACHE INTERNAL "" FORCE)
  if(NOT EMSCRIPTEN)
    set(OSSIA_PROTOCOL_ARTNET ON CACHE INTERNAL "" FORCE)
    set(OSSIA_PROTOCOL_WIIMOTE ON CACHE INTERNAL "" FORCE)
    set(OSSIA_PROTOCOL_MQTT5 ON CACHE INTERNAL "" FORCE)
    set(OSSIA_PROTOCOL_COAP ON CACHE INTERNAL "" FORCE)
  else()
    set(OSSIA_PROTOCOL_ARTNET OFF CACHE INTERNAL "" FORCE)
    set(OSSIA_PROTOCOL_WIIMOTE OFF CACHE INTERNAL "" FORCE)
    set(OSSIA_PROTOCOL_MQTT5 OFF CACHE INTERNAL "" FORCE)
    set(OSSIA_PROTOCOL_COAP OFF CACHE INTERNAL "" FORCE)
  endif()
endif()

set(LIBREMIDI_NO_WINUWP ON CACHE INTERNAL "" FORCE)
set(LIBREMIDI_NO_WINMIDI ON CACHE INTERNAL "" FORCE)
set(LIBREMIDI_NO_NETWORK ON CACHE INTERNAL "" FORCE)
set(LIBREMIDI_TESTS OFF CACHE INTERNAL "" FORCE)
set(LIBREMIDI_EXAMPLES OFF CACHE INTERNAL "" FORCE)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  set(OSSIA_SUBMODULE_AUTOUPDATE OFF CACHE INTERNAL "" FORCE)
else()
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libossia/cmake/cmake-modules/.git")
    set(OSSIA_SUBMODULE_AUTOUPDATE ON CACHE INTERNAL "" FORCE)
  else()
    set(OSSIA_SUBMODULE_AUTOUPDATE OFF CACHE INTERNAL "" FORCE)
  endif()
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libossia")
