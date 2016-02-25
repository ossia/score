set(CMAKE_C_COMPILER /usr/bin/clang)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};/usr/jamoma/share/cmake/Jamoma")

include(travis/debug)
include(all-plugins)
