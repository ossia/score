cninja_require(lld)
#cninja_require(linkerwarnings)
cninja_require(score-warnings)

set(CMAKE_BUILD_TYPE Debug)
set_cache(SCORE_STATIC_PLUGINS True)
set_cache(CMAKE_UNITY_BUILD True)
set_cache(SCORE_PCH False)
