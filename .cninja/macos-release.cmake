cninja_require(compiler=xcode)
cninja_require(static-release)
cninja_require(linker-warnings=no)
cninja_require(era=11.0)

execute_process(
  COMMAND xcode-select --print-path
  OUTPUT_VARIABLE XCODE_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
set_cache(CMAKE_OSX_SYSROOT "${XCODE_PATH}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk")

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
  string(APPEND CMAKE_C_FLAGS_INIT "  -mtune=cannonlake  ")
  string(APPEND CMAKE_CXX_FLAGS_INIT " -mtune=cannonlake ")
  set_cache(KFR_ARCH sse2)
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")
  set_cache(KFR_ARCH neon)
endif()

set_cache(BUILD_SHARED_LIBS OFF)
set_cache(CMAKE_INSTALL_MESSAGE NEVER)

set_cache(SCORE_DEPLOYMENT_BUILD 1)
set_cache(SCORE_INSTALL_HEADERS ON)
set_cache(OSSIA_STATIC_EXPORT ON)
