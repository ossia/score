cninja_require(compiler=clang)
cninja_require(lld)
cninja_require(score-warnings)

set_cache(CMAKE_BUILD_TYPE Release)
set_cache(SCORE_STATIC_PLUGINS True)
set_cache(CMAKE_UNITY_BUILD True)
set_cache(SCORE_PCH False)

# one day: -fsingle-global-definition
if(NOT APPLE)
  string(APPEND CMAKE_CXX_FLAGS_INIT " -fnew-infallible  -fno-semantic-interposition  ")
endif()

string(APPEND CMAKE_C_FLAGS_INIT " -fno-stack-protector -Ofast -fno-finite-math-only -fno-plt -Bsymbolic-functions ")
string(APPEND CMAKE_CXX_FLAGS_INIT " -fno-stack-protector -Ofast -fno-finite-math-only -fno-plt -Bsymbolic-functions ")
