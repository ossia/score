set(CMAKE_BUILD_TYPE Release)
set(ISCORE_COTIRE True)
set(DEPLOYMENT_BUILD True)

if(NOT DEFINED ISCORE_ENABLE_LTO)
set(ISCORE_ENABLE_LTO True)
endif()
if(UNIX AND EXISTS "/etc/centos-release")
  set(ISCORE_ENABLE_LTO False)
endif()

include(default-plugins)
