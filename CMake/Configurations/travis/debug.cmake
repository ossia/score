set(CMAKE_BUILD_TYPE Debug CACHE INTERNAL "")
set(SCORE_COTIRE True)

if(NOT DEFINED DEPLOYMENT_BUILD)
  set(DEPLOYMENT_BUILD False)
endif()

include(default-plugins)
