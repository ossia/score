cninja_require(lld)
cninja_require(linkerwarnings)
cninja_require(lto=full)
cninja_require(debugmode)
cninja_require(score-warnings)

set_cache(SCORE_DYNAMIC_PLUGINS True)
set_cache(CMAKE_UNITY_BUILD True)
