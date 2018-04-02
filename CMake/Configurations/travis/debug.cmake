set(CMAKE_BUILD_TYPE Debug)
if(NOT DEFINED SCORE_COTIRE)
set(SCORE_COTIRE True)
endif()
set(DEPLOYMENT_BUILD False)

include(default-plugins)
