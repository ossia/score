cninja_require(static-release)

set_cache(DEPLOYMENT_BUILD 1)
set_cache(CMAKE_SKIP_RPATH 1)
set_cache(BUILD_SHARED_LIBS OFF)
set_cache(CMAKE_FIND_LIBRARY_SUFFIXES .a)
set_cache(SCORE_INSTALL_HEADERS ON)

add_linker_flags(" -static-libgcc -static-libstdc++ -Wl,--version-script,/score/cmake/Deployment/Linux/AppImage/version")

string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " -pthread")

