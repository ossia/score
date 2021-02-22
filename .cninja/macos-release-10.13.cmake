cninja_require(compiler=xcode)
cninja_require(static-release)
cninja_require(linkerwarnings=no)
cninja_require(era=10.13)

set_cache(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

string(APPEND CMAKE_C_FLAGS_INIT " -march=core2 -mtune=ivybridge")
string(APPEND CMAKE_CXX_FLAGS_INIT " -march=core2 -mtune=ivybridge -D_LIBCPP_NO_EXCEPTIONS=1")
string(APPEND CMAKE_OBJCXX_FLAGS_INIT " -march=core2 -mtune=ivybridge -D_LIBCPP_NO_EXCEPTIONS=1")


set_cache(BUILD_SHARED_LIBS OFF)
set_cache(CMAKE_INSTALL_MESSAGE NEVER)

set_cache(DEPLOYMENT_BUILD 1)
set_cache(SCORE_INSTALL_HEADERS ON)
set_cache(OSSIA_STATIC_EXPORT ON)
