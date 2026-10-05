# static-release.cmake with every rule kept but the optimization: -O0 and full
# debug info, to reproduce under a debugger what only a release build does.
#
# The build type stays Release: NDEBUG, no SCORE_DEBUG, and none of the
# -Ofast -march=native that score_optimize_in_debug_mode applies in Debug.
# -ffast-math stays: it changes semantics, not only speed (__FAST_MATH__ in
# headers, crtfastmath.o enabling flush-to-zero for the whole process).
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
