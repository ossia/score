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
  
  file(CONFIGURE 
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
    CONTENT "${SCORE_QTCONF_CONTENT}"
    NEWLINE_STYLE UNIX
  )
  
  file(CONFIGURE 
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/qtconf.qrc"
    CONTENT 
"<RCC>
  <qresource prefix=\"/\">
    <file alias=\"qt/etc/qt.conf\">qt.conf</file>
  </qresource>
</RCC>
"
    NEWLINE_STYLE UNIX
  )
else()
  file(CONFIGURE 
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/qtconf.qrc"
    CONTENT 
"<RCC>
  <qresource prefix=\"/\">
  </qresource>
</RCC>
"
    NEWLINE_STYLE UNIX
  )
endif()