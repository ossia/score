set(CMAKE_BUILD_TYPE Release)
set(ISCORE_COTIRE True)
set(DEPLOYMENT_BUILD True)

if(NOT DEFINED ISCORE_ENABLE_LTO)
set(ISCORE_ENABLE_LTO True)
endif()

include(default-plugins)
