if(SCORE_DEPLOYMENT_BUILD)
  if(WIN32)
    set(SCORE_QTCONF_CONTENT 
"[Paths]
Prefix = .

[Platforms]
WindowsArguments = fontengine=freetype
")
  elseif(APPLE)
    set(SCORE_QTCONF_CONTENT 
"[Paths]
Plugins = PlugIns
Qml2Imports = Resources/qml

[Platforms]
CocoaArguments = fontengine=freetype
")
  else()
    set(SCORE_QTCONF_CONTENT 
"[Paths]
Prefix = .
")
  endif()
  
  file(
    WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
    "${SCORE_QTCONF_CONTENT}"
  )
  
  configure_file("${SCORE_ROOT_SOURCE_DIR}/cmake/qtconf.used.qrc" "${CMAKE_CURRENT_BINARY_DIR}/qtconf.qrc")
else()
  configure_file("${SCORE_ROOT_SOURCE_DIR}/cmake/qtconf.empty.qrc" "${CMAKE_CURRENT_BINARY_DIR}/qtconf.qrc")
endif()
