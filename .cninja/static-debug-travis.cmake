cninja_require(static-debug)

string(APPENd CMAKE_C_FLAGS " -g0 -Og -s")
string(APPENd CMAKE_CXX_FLAGS " -g0 -Og -s")
string(APPENd CMAKE_C_FLAGS_DEBUG " -g0 -Og -s")
string(APPENd CMAKE_CXX_FLAGS_DEBUG " -g0 -Og -s")
string(APPENd CMAKE_EXE_LINKER_FLAGS " -Og -Wl,--strip-all")