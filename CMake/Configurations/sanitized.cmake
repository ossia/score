# set(CMAKE_C_COMPILER /usr/bin/clang)
# set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(SCORE_COTIRE_DISABLE_UNITY True)
set(SCORE_SANITIZE True)
set(SCORE_STATIC_PLUGINS True)

#set(SCORE_DISABLE_ADDONS True)
include(all-plugins)

set(SCORE_COTIRE True)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(SCORE_COTIRE False)
endif()
