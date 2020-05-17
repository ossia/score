#!/bin/bash

install_name_tool -add_rpath @executable_path/../Frameworks "$1/MacOS/score"
install_name_tool -add_rpath @executable_path/../Frameworks "$1/MacOS/ossia-score-vstpuppet"

# Also fixup for the QtQuick dylibs
find "$1/Resources/qml" -name '*.dylib' -exec rm {} \;
find "$1" -name '*dSYM' -exec rm -rf {} \;
find "$1" -name '*_debug*' -exec rm -rf {} \;
find "$1" -name '*qmlc' -exec rm -rf {} \;
(
  cd "$1/Frameworks"
  rm -rf Qt3DCore.framework Qt3DInput.framework Qt3DLogic.framework Qt3DQuick.framework Qt3DQuickScene2D.framework Qt3DRender.framework QtXmlPatterns.framework QtSql.framework QtConcurrent.framework QtGamepad.framework QtQuickParticles.framework
)
