cninja_require(static-debug)

set_cache(DEPLOYMENT_BUILD 1)
set_cache(CMAKE_SKIP_RPATH 1)
set_cache(BUILD_SHARED_LIBS OFF)
set_cache(CMAKE_FIND_LIBRARY_SUFFIXES .a)
set_cache(SCORE_INSTALL_HEADERS ON)
set_cache(OSSIA_STATIC_EXPORT ON)
set_cache(CMAKE_INSTALL_MESSAGE NEVER)

add_linker_flags(" -Wl,--version-script,/score/cmake/Deployment/Linux/AppImage/version")

# Note: libc++ does not export its symbols from its static lib which prevents usage with jit...
#add_linker_flags(" -static-libgcc -static-libstdc++ -Wl,--version-script,/score/cmake/Deployment/Linux/AppImage/version")
#add_linker_flags(" -static-libgcc -static-libstdc++")

string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " -pthread")

