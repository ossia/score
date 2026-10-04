# The release AppImage build (appimage.cmake over static-release.cmake) with
# every rule kept but the optimization: -O0 and full debug info, to reproduce
# under a debugger what only the release build does.
#
# The build type stays Release: NDEBUG, no SCORE_DEBUG, and no
# score_optimize_in_debug_mode, which compiles some targets with -Ofast and
# -march=native in Debug. -ffast-math stays too, as -Ofast implies it: it is
# more than an optimization (__FAST_MATH__ in headers, crtfastmath.o setting
# flush-to-zero for the whole process).
cninja_require(compiler=clang)
cninja_require(lld)
cninja_require(score-warnings)

set_cache(CMAKE_BUILD_TYPE Release)
set_cache(CMAKE_C_FLAGS_RELEASE "-O0 -g -DNDEBUG")
set_cache(CMAKE_CXX_FLAGS_RELEASE "-O0 -g -DNDEBUG")
set_cache(SCORE_STATIC_PLUGINS True)
set_cache(CMAKE_UNITY_BUILD True)
set_cache(SCORE_PCH False)

if(NOT APPLE)
  string(APPEND CMAKE_CXX_FLAGS_INIT " -fnew-infallible  -fno-semantic-interposition  ")
endif()

string(APPEND CMAKE_C_FLAGS_INIT " -fno-stack-protector -ffast-math -fno-finite-math-only -fno-plt -Bsymbolic-functions ")
string(APPEND CMAKE_CXX_FLAGS_INIT " -fno-stack-protector -ffast-math -fno-finite-math-only -fno-plt -Bsymbolic-functions ")

# From here on, appimage.cmake.
set_cache(SCORE_DEPLOYMENT_BUILD 1)
set_cache(SCORE_STRICT_FEATURE_CHECK 1)
set_cache(CMAKE_SKIP_RPATH 1)
set_cache(BUILD_SHARED_LIBS OFF)
set_cache(CMAKE_FIND_LIBRARY_SUFFIXES .a)
set_cache(SCORE_INSTALL_HEADERS ON)
set_cache(OSSIA_STATIC_EXPORT ON)
set_cache(CMAKE_INSTALL_MESSAGE NEVER)

set_cache(CMAKE_C_VISIBILITY_PRESET default)
set_cache(CMAKE_CXX_VISIBILITY_PRESET default)
set_cache(CMAKE_VISIBILITY_INLINES_HIDDEN 0)

add_linker_flags(" -Wl,--version-script,/score/cmake/Deployment/Linux/AppImage/version")

string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " -pthread")
