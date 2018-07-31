if(UNIX)

#use the LSB stuff if possible :)
set(LSB_ID "")
set(LSB_VER "")

if(EXISTS /etc/lsb-release)
  execute_process(
    COMMAND cat /etc/lsb-release
    COMMAND grep DISTRIB_ID
    COMMAND awk -F= "{ print $2 }"
    COMMAND tr "\n" " "
    COMMAND sed "s/ //"
    OUTPUT_VARIABLE LSB_ID
    RESULT_VARIABLE LSB_ID_RESULT
    )

  execute_process(
    COMMAND cat /etc/lsb-release
    COMMAND grep DISTRIB_RELEASE
    COMMAND awk -F= "{ print $2 }"
    COMMAND tr "\n" " "
    COMMAND sed "s/ //"
    OUTPUT_VARIABLE LSB_VER
    RESULT_VARIABLE LSB_VER_RESULT
    )

  if(LSB_ID_RESULT EQUAL 1)
    set(LSB_ID "")
    set(LSB_VER "")
  endif()
endif()


if(NOT "${LSB_ID}" STREQUAL "")
    set(INSTALLER_PLATFORM "${LSB_ID}-${LSB_VER}" CACHE PATH "Installer chosen platform")
else()
    set(INSTALLER_PLATFORM "linux-generic" CACHE PATH "Installer chosen platform")
endif()

set(CPACK_SYSTEM_NAME "${INSTALLER_PLATFORM}")

if("${LSB_ID}" STREQUAL "Debian" OR "${LSB_ID}" STREQUAL "Ubuntu" OR "${LSB_ID}" STREQUAL "Mint")
    set(CPACK_GENERATOR "DEB")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

if(SCORE_STATIC_QT)
configure_file (
  "${CMAKE_CURRENT_LIST_DIR}/Deployment/Linux/ossia-score.static.desktop.in"
  "${PROJECT_BINARY_DIR}/ossia-score.desktop"
  )
else()
configure_file (
  "${CMAKE_CURRENT_LIST_DIR}/Deployment/Linux/ossia-score.desktop.in"
  "${PROJECT_BINARY_DIR}/ossia-score.desktop"
  )
endif()

if(EXISTS "${CMAKE_BINARY_DIR}/base/plugins/score-plugin-media/faustlibs/src/faustlibs")
  install(
    DIRECTORY
      "${CMAKE_BINARY_DIR}/base/plugins/score-plugin-media/faustlibs-prefix/src/faustlibs/"
    DESTINATION
      "share/faust"
   )
endif()

install(FILES "${PROJECT_BINARY_DIR}/ossia-score.desktop"
        DESTINATION share/applications)
install(FILES "${CMAKE_SOURCE_DIR}/base/lib/resources/ossia-score.png"
        DESTINATION share/pixmaps)

set(CPACK_PACKAGE_FILE_NAME "score-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
set(CPACK_PACKAGING_INSTALL_PREFIX "")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ossia devs <iscore-devs@lists.sourceforge.net>")
if(SCORE_STATIC_QT)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
else()
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5core5a, libqt5gui5, libqt5svg5, libqt5xml5, libqt5network5")
endif()
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")


endif()
