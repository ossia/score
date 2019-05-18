include(mingw-static-release)
set(BOOST_ROOT "$ENV{BOOST_ROOT}")
set(BOOST_LIBRARYDIR "$ENV{BOOST_LIBRARYDIR}")
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{QTDIR}/lib/cmake/Qt5;$ENV{JAMOMA_ROOT}/share/cmake/jamoma")


