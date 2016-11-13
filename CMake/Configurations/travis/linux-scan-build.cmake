set(CMAKE_BUILD_TYPE Release)

set(DISABLE_COTIRE True CACHE BOOL "No cotire" FORCE)
set(OSSIA_SANITIZE True CACHE BOOL "Sanitize ossia" FORCE)
set(ISCORE_COTIRE False)
set(DEPLOYMENT_BUILD False)
set(ISCORE_COVERAGE False)
set(ISCORE_ENABLE_LTO True)
include(all-plugins)
