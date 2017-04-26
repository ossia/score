set(CMAKE_BUILD_TYPE Debug)
if(NOT DEFINED ISCORE_COTIRE)
  set(ISCORE_COTIRE True)
endif()
set(DEPLOYMENT_BUILD False)

include(default-plugins)
