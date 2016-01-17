set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};/usr/local/jamoma/share/Cmake/Jamoma")
set(ISCORE_COTIRE_DISABLE_UNITY True)

include(travis/debug)
include(all-plugins)
