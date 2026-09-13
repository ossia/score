if(NOT SCORE_DEPLOYMENT_BUILD)
    return()
endif()

set(CPACK_PACKAGE_NAME "ossia score")
set(CPACK_PACKAGE_VENDOR "ossia")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "An intermedia sequencer for the precise and flexible scripting of interactive scenarios.")
set(CPACK_PACKAGE_VERSION "${SCORE_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${SCORE_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${SCORE_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${SCORE_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION_EXTRA "${SCORE_VERSION_EXTRA}")
set(CPACK_RESOURCE_FILE_LICENSE "${SCORE_ROOT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_SOURCE_GENERATOR TGZ)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "ossia-score")

set(CPACK_SOURCE_IGNORE_FILES
  "\\\\.#;/#;.*~"
  "\\\\.git"
  "/\\\\.idea"
  ".*\\\\.user"
  "max-sdk"
  "pure-data"
  "/src/addons/score.*"
  "/src/disabled_addons"
  "/src/disabled-addons"
  "/Documentation"
  "/3rdparty/ossia/3rdparty/concurrentqueue/benchmarks"
)

set(CPACK_INSTALL_CMAKE_PROJECTS)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  include(ScoreDeploymentOSX)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
  include(ScoreDeploymentAndroid)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  include(ScoreDeploymentLinux)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
include(ScoreDeploymentWindows)
include(ScoreDeploymentWindowsStore)
endif()

if(SCORE_INSTALL_HEADERS)
  # Out-of-tree add-ons have no way to see the version from the SDK otherwise,
  # and a hardcoded copy in ScoreExternalAddon.cmake goes stale unnoticed.
  file(CONFIGURE
    OUTPUT "${CMAKE_BINARY_DIR}/ScoreVersion.cmake"
    CONTENT "set(SCORE_VERSION_MAJOR ${SCORE_VERSION_MAJOR})
set(SCORE_VERSION_MINOR ${SCORE_VERSION_MINOR})
set(SCORE_VERSION_PATCH ${SCORE_VERSION_PATCH})
set(SCORE_VERSION_EXTRA \"${SCORE_VERSION_EXTRA}\")
set(SCORE_VERSION \"${SCORE_VERSION}\")
")

  install(
    FILES
      ${CMAKE_BINARY_DIR}/ScoreVersion.cmake
      ${SCORE_ROOT_SOURCE_DIR}/cmake/ScoreAddonArchitecture.cmake
      ${SCORE_ROOT_SOURCE_DIR}/cmake/ScoreAddonSetup.cmake
      ${SCORE_ROOT_SOURCE_DIR}/cmake/ScoreAvndHelper.cmake
      ${SCORE_ROOT_SOURCE_DIR}/cmake/ScoreExternalAddon.cmake
      ${SCORE_ROOT_SOURCE_DIR}/cmake/ScoreExternalAddon.developer.cmake
      ${SCORE_ROOT_SOURCE_DIR}/cmake/ScoreExternalAddon.sdk.cmake
      ${SCORE_ROOT_SOURCE_DIR}/cmake/ScoreFunctions.cmake
      ${SCORE_ROOT_SOURCE_DIR}/cmake/ScoreTargetSetup.cmake
    DESTINATION lib/cmake/score
    COMPONENT Devel
    OPTIONAL
  )

  # Add-ons converging on find_package(Avendish) + avnd_addon_* need Avendish's
  # CMake package, not just the headers create-sdk-common.sh copies. Callers
  # pass -DCMAKE_MODULE_PATH=<sdk>/lib/cmake/score without touching
  # CMAKE_PREFIX_PATH, so the entry point is a FindAvendish module there rather
  # than a config package: find_package() tries module mode first, and this
  # keeps it working without every add-on or CI caller changing its flags.
  file(CONFIGURE
    OUTPUT "${CMAKE_BINARY_DIR}/FindAvendish.cmake"
    CONTENT "get_filename_component(_avnd_sdk_root \"\${CMAKE_CURRENT_LIST_DIR}/../avendish\" ABSOLUTE)
if(EXISTS \"\${_avnd_sdk_root}/AvendishConfig.cmake\")
  include(\"\${_avnd_sdk_root}/AvendishConfig.cmake\")
else()
  set(Avendish_FOUND FALSE)
endif()
")

  install(
    FILES
      ${CMAKE_BINARY_DIR}/FindAvendish.cmake
    DESTINATION lib/cmake/score
    COMPONENT Devel
    OPTIONAL
  )

  install(
    FILES
      ${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/AvendishConfig.cmake
    DESTINATION lib/cmake/avendish
    COMPONENT Devel
    OPTIONAL
  )
  install(
    FILES
      ${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/cmake/AvendishAddon.cmake
    DESTINATION lib/cmake/avendish/cmake
    COMPONENT Devel
    OPTIONAL
  )
endif()


include(CPack)
