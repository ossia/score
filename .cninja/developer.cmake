cninja_require(compiler=clang)
cninja_require(lld)
cninja_require(linkerwarnings)
cninja_require(debugsyms)
cninja_require(debugsplit)
cninja_require(score-warnings)
#cninja_require(debugmode)

set_cache(CMAKE_BUILD_TYPE Debug)
set_cache(CMAKE_LINK_DEPENDS_NO_SHARED 1)
set_cache(SCORE_PCH True)
set_cache(SCORE_DYNAMIC_PLUGINS True)

if(UNIX AND NOT APPLE)
  string(APPEND CMAKE_C_FLAGS_INIT " -ggnu-pubnames -fdebug-types-section")
  string(APPEND CMAKE_CXX_FLAGS_INIT " -ggnu-pubnames -fdebug-types-section")
endif()

