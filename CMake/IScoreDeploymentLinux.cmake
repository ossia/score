if(UNIX)

#use the LSB stuff if possible :)
EXECUTE_PROCESS(
  COMMAND cat /etc/lsb-release
  COMMAND grep DISTRIB_ID
  COMMAND awk -F= "{ print $2 }"
  COMMAND tr "\n" " "
  COMMAND sed "s/ //"
  OUTPUT_VARIABLE LSB_ID
  RESULT_VARIABLE LSB_ID_RESULT
)

EXECUTE_PROCESS(
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

install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/i-score.sh"
        DESTINATION "."
        COMPONENT DynamicRuntimeHelper)

set(CPACK_PACKAGE_FILE_NAME "i-score-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
set(CPACK_PACKAGING_INSTALL_PREFIX "")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "i-score devs <i-score-devs@lists.sourceforge.net>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5core5a, libqt5gui5, libqt5svg5, libqt5xml5, libqt5network5, libqt5printsupport5")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "jamomacore")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")

endif()
