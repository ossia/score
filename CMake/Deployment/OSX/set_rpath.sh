#!/bin/bash 
PLUGIN_FOLDER=$1/MacOS/plugins
for PLUGIN in $PLUGIN_FOLDER/*.dylib; 
do
  OTOOL_OUTPUT=$(otool -L "$PLUGIN")
  LIBS=$(echo "$OTOOL_OUTPUT" | grep 'libscore_plugin' | grep -v ':' | awk '{print $1}')

  echo "Fixing $PLUGIN :"
  install_name_tool -id "@rpath/$PLUGIN" "$PLUGIN"
  
  for LIB in $LIBS
  do
        FIXED_LIB=$(echo "$LIB" | perl -pe 's|(.*?libscore_)|\@rpath/libscore_|')
        echo $LIB
        echo $FIXED_LIB
        install_name_tool -change "$LIB" "$FIXED_LIB" "$PLUGIN"
  done   
done
install_name_tool -add_rpath @executable_path/plugins "$1/MacOS/score"
install_name_tool -add_rpath @executable_path/../Frameworks "$1/MacOS/score"

# Also fixup for the QtQuick dylibs
find "$1/Resources/qml" -name '*.dylib' -exec rm {} \;
find "$1" -name '*dSYM' -exec rm {} \;
find "$1" -name '*_debug' -exec rm {} \;
ln -s "$1/PlugIns/quick/libqtquick2plugin.dylib" "$1/Resources/qml/QtQuick.2"
