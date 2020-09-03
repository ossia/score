cninja_require(clang)
cninja_require(lld)
cninja_require(linkerwarnings)
cninja_require(score-warnings)

set_cache(CMAKE_BUILD_TYPE Release)
set_cache(SCORE_STATIC_PLUGINS True)
set_cache(CMAKE_UNITY_BUILD True)
set_cache(SCORE_PCH False)

string(APPEND CMAKE_C_FLAGS_INIT " -fno-stack-protector -Ofast -fno-finite-math-only ")
string(APPEND CMAKE_CXX_FLAGS_INIT " -fno-stack-protector -Ofast -fno-finite-math-only ")
