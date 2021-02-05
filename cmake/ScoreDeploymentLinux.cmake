if(UNIX)
set(CPACK_PACKAGE_NAME "ossia-score")

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
elseif(EXISTS /usr/bin/lsb_release)
  execute_process(
    COMMAND lsb_release -s -i
    COMMAND tr "\n" " "
    COMMAND sed "s/ //"
    OUTPUT_VARIABLE LSB_ID
    RESULT_VARIABLE LSB_ID_RESULT
    )
  execute_process(
    COMMAND lsb_release -s -r
    COMMAND tr "\n" " "
    COMMAND sed "s/ //"
    OUTPUT_VARIABLE LSB_VER
    RESULT_VARIABLE LSB_VER_RESULT
    )
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

configure_file (
  "${CMAKE_CURRENT_LIST_DIR}/Deployment/Linux/ossia-score.desktop.in"
  "${PROJECT_BINARY_DIR}/ossia-score.desktop"
  )

if(EXISTS "${CMAKE_BINARY_DIR}/src/plugins/score-plugin-faust/faustlibs-prefix/src/faustlibs")
  install(
    DIRECTORY
      "${CMAKE_BINARY_DIR}/src/plugins/score-plugin-faust/faustlibs-prefix/src/faustlibs/"
    DESTINATION
      "share/faust"
    COMPONENT OssiaScore
     PATTERN ".git" EXCLUDE
     PATTERN "doc" EXCLUDE
     PATTERN "*.html" EXCLUDE
     PATTERN "*.svg" EXCLUDE
     PATTERN "*.scad" EXCLUDE
     PATTERN "*.obj" EXCLUDE
     PATTERN "build" EXCLUDE
     PATTERN ".gitignore" EXCLUDE
     PATTERN "modalmodels" EXCLUDE
   )
endif()

install(FILES "${PROJECT_BINARY_DIR}/ossia-score.desktop"
        DESTINATION share/applications
        COMPONENT OssiaScore)
install(FILES "${CMAKE_SOURCE_DIR}/src/lib/resources/ossia-score.png"
        DESTINATION share/pixmaps
        COMPONENT OssiaScore)


execute_process(
    COMMAND
        dpkg --print-architecture
    OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
    RESULT_VARIABLE dpkg_ok
)
string(STRIP "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}" CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
if(NOT (dpkg_ok EQUAL 0))
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
endif()

set(CPACK_PACKAGING_INSTALL_PREFIX "")

set(CPACK_DEBIAN_PACKAGE_NAME "ossia-score")
set(CPACK_DEBIAN_OSSIASCORE_PACKAGE_NAME "ossia-score")
set(CPACK_DEBIAN_FILE_NAME "ossia-score_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ossia devs <contact@ossia.io>")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://ossia.io")
set(CPACK_DEBIAN_PACKAGE_PROVIDES "ossia-score")
if(SCORE_STATIC_QT)
  #set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1-mesa-glx, libpng16-16, libc6, zlib1g, libpcre2-16-0, libdbus-1-3, libjpeg62-turbo, libfontconfig1, libfreetype6, libx11-xcb1, libxcb-render-util0, libxcb-render0, libxcb-xkb1, libxrender1, libxext6, libx11-xcb1, libsm6, libxcb-sync1, libxcb-xfixes0, libxcb-xinerama0, libxcb-randr0, libxcb-image0, libxcb-shm0, libxcb-keysyms1, libxcb-icccm4, libxcb-shape0, libxcb1, libxkbcommon-x11-0, libxkbcommon0, libxcb-glx0, libasound2, libudev1")
endif()
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_STRIP_FILES "bin/ossia-score;bin/ossia-score-vstpuppet")
set(CPACK_COMPONENTS_ALL OssiaScore)
endif()
