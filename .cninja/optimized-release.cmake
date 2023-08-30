cninja_require(static-release)

string(APPEND CMAKE_C_FLAGS_INIT " -march=native -flto ")
string(APPEND CMAKE_CXX_FLAGS_INIT " -march=native -flto ")
