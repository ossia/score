
set(CPACK_INSTALL_STRIP ON)
set(CPACK_STRIP_FILES ON)
set(CMAKE_INSTALL_DO_STRIP ON)

set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_EXECUTABLES "score.exe;ossia score")

set(CPACK_COMPONENTS_ALL OssiaScore)
set(CPACK_MONOLITHIC_INSTALL FALSE)

set(CPACK_NSIS_PACKAGE_NAME "ossia score")
set(CPACK_NSIS_INSTALLED_ICON_NAME "score.exe")
set(CPACK_PACKAGE_ICON "${SCORE_ROOT_SOURCE_DIR}\\\\src\\\\lib\\\\resources\\\\score.ico")
set(CPACK_NSIS_MUI_ICON "${CPACK_PACKAGE_ICON}")
set(CPACK_NSIS_MUI_UNIICON "${CPACK_PACKAGE_ICON}")
set(CPACK_NSIS_MANIFEST_DPI_AWARE 1)

set(CPACK_NSIS_HELP_LINK "https:\\\\\\\\ossia.io")
set(CPACK_NSIS_URL_INFO_ABOUT "https:\\\\\\\\ossia.io")
set(CPACK_NSIS_CONTACT "https:\\\\\\\\gitter.im\\\\OSSIA\\\\score")

set(CPACK_NSIS_COMPRESSOR "/SOLID lzma")
set(CPACK_NSIS_BRANDING_TEXT " ")

set(CPACK_NSIS_MENU_LINKS
    "score.exe" "ossia score"
    "https://ossia.io" "ossia website"
    )


set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
    "!include ${CMAKE_CURRENT_LIST_DIR}\\\\Deployment\\\\Windows\\\\FileAssociation.nsh"
)

# The description passed to these macros is the ProgID: registration and
# unregistration have to spell it identically or the key is orphaned.
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
\\\${registerExtension} '\\\$INSTDIR\\\\score.exe' '.score' 'ossia score document'
\\\${registerExtension} '\\\$INSTDIR\\\\score.exe' '.scorejson' 'ossia score document'
\\\${registerExtension} '\\\$INSTDIR\\\\score.exe' '.scorebin' 'ossia score document'

SetOutPath '\\\$INSTDIR'
CreateShortcut '\\\$DESKTOP\\\\score.lnk' '\\\$INSTDIR\\\\score.exe' '' '\\\$INSTDIR\\\\score.ico'
SetRegView 64
WriteRegStr HKEY_LOCAL_MACHINE 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\score.exe' '' '$INSTDIR\\\\score.exe'
WriteRegStr HKEY_LOCAL_MACHINE 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\score.exe' 'Path' '$INSTDIR;$INSTDIR\\\\plugins\\\\;'
")

set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
Delete '$DESKTOP\\\\score.lnk'
DeleteRegKey HKLM 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\score.exe'
\\\${unregisterExtension} '.score' 'ossia score document'
\\\${unregisterExtension} '.scorejson' 'ossia score document'
\\\${unregisterExtension} '.scorebin' 'ossia score document'
DeleteRegKey HKCR 'score file'
")
