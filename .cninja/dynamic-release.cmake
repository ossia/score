cninja_require(compiler=clang)
cninja_require(lld)
cninja_require(linker-warnings)
cninja_require(score-warnings)

set_cache(CMAKE_BUILD_TYPE Release)
set_cache(SCORE_DYNAMIC_PLUGINS True)
set_cache(CMAKE_SKIP_RPATH False)
set_cache(SCORE_PCH False)

string(APPEND CMAKE_C_FLAGS_INIT " -g3 -fno-stack-protector -Ofast -fno-finite-math-only  -fnew-infallible  -fno-semantic-interposition  -fno-plt -Bsymbolic-functions  ")
string(APPEND CMAKE_CXX_FLAGS_INIT " -g3 -fno-stack-protector -Ofast -fno-finite-math-only  -fnew-infallible  -fno-semantic-interposition   -fno-plt -Bsymbolic-functions  ")
