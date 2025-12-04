cninja_require(compiler=xcode)
cninja_require(static-release)
cninja_require(linker-warnings=no)
cninja_require(era=12.0)

execute_process(
  COMMAND xcode-select --print-path
  OUTPUT_VARIABLE XCODE_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
set_cache(CMAKE_OSX_SYSROOT "${XCODE_PATH}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk")

if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
  set(IS_ARM64 0)
elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
  set(IS_ARM64 1)
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
  set(IS_ARM64 0)
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")
  set(IS_ARM64 1)
endif()

if(IS_ARM64)
  set_cache(KFR_ARCH neon64)
else()
  string(APPEND CMAKE_C_FLAGS_INIT "  -march=x86-64-v2  ")
  string(APPEND CMAKE_CXX_FLAGS_INIT "  -march=x86-64-v2 ")
  set_cache(KFR_ARCH avx)
  set_cache(KFR_ARCHS "sse41;sse42;avx;avx2;avx512")
endif()

set_cache(BUILD_SHARED_LIBS OFF)
set_cache(CMAKE_INSTALL_MESSAGE NEVER)

set_cache(SCORE_PCH 0)
set_cache(SCORE_DEPLOYMENT_BUILD 1)
set_cache(SCORE_INSTALL_HEADERS ON)
set_cache(OSSIA_STATIC_EXPORT ON)
set_cache(SCORE_MACOS_ONLY_SYSTEM_LIBARIES ON)
