#!/bin/bash 

for PLUGIN in $1/*.dylib; 
do
  OTOOL_OUTPUT=$(otool -L "$PLUGIN")
  LIBS=$(echo "$OTOOL_OUTPUT" | grep 'libiscore_plugin' | grep -v ':' | awk '{print $1}')

  echo "Fixing $PLUGIN :"
  install_name_tool -id "@rpath/$PLUGIN" "$PLUGIN"
  
  for LIB in $LIBS
  do
        FIXED_LIB=$(echo "$LIB" | perl -pe 's|(.*?libiscore_)|\@rpath/libiscore_|')
        echo $LIB
        echo $FIXED_LIB
        install_name_tool -change "$LIB" "$FIXED_LIB" "$PLUGIN"
  done   
done
install_name_tool -add_rpath @executable_path/plugins "$1/../i-score"
