#!/bin/bash 

# $1 shall be $install_prefix/i-score.app/Contents/MacOS/i-score/plugins
# for PLUGIN in $1/*.dylib; 
# do
#   OTOOL_OUTPUT=$(otool -L "$PLUGIN")
#   if [[ $(echo "$OTOOL_OUTPUT" | grep -cEv '(rpath|executable_path|/usr/lib|:)') > 0 ]]; then
#     LIBS=$(echo "$OTOOL_OUTPUT" | grep -Ev '(rpath|executable_path|/usr/lib|:)' | awk '{print $1}')
#     for LIB in $LIBS
#     do
#       if [[ $(echo "$LIB" | grep -c executable_path) = 0 ]];
#       then
#         # Replace the Qt libs
#         if [[ $(echo "$LIB" | grep -c Qt) > 0 ]];
#         then
#   	      FIXED_LIB=$(echo "$LIB" | perl -pe 's|(.*?Qt)|\@executable_path/../Frameworks/Qt|')
#           install_name_tool -change "$LIB" "$FIXED_LIB" "$PLUGIN"
#         fi

#      	# Replace Jamoma libs
#         if [[ $(echo "$LIB" | grep -c libAPIJamoma ) > 0 ]]; then
#           install_name_tool -change "$LIB" "@rpath/libAPIJamoma.dylib" "$PLUGIN"
#         fi
#       fi
#     done
   
#    echo "Processing $PLUGIN file.."

#   fi
# done

for PLUGIN in $1/*.dylib; 
do
  OTOOL_OUTPUT=$(otool -L "$PLUGIN")
  LIBS=$(echo "$OTOOL_OUTPUT" | grep 'libiscore_plugin' | grep -v ':' | awk '{print $1}')

  echo "Fixing $PLUGIN :"
  install_name_tool -id "@rpath/$PLUGIN" "$PLUGIN"

  if [[ $(echo "$LIBS" | grep -c 'MacOS/libJamoma') ]];
  then
    install_name_tool -change "@executable_path/../MacOS/libJamomaFoundation.6.dylib" "@rpath/libJamomaFoundation.6.dylib" "$PLUGIN"
    install_name_tool -change "@executable_path/../MacOS/libJamomaModular.6.dylib" "@rpath/libJamomaModular.6.dylib" "$PLUGIN"
    install_name_tool -change "@executable_path/../MacOS/libJamomaDSP.6.dylib" "@rpath/libJamomaDSP.6.dylib" "$PLUGIN"
  fi
  for LIB in $LIBS
  do
        FIXED_LIB=$(echo "$LIB" | perl -pe 's|(.*?libiscore_)|\@rpath/libiscore_|')
        echo $LIB
        echo $FIXED_LIB
        install_name_tool -change "$LIB" "$FIXED_LIB" "$PLUGIN"
  done   
done
install_name_tool -change "@executable_path/../MacOS/libJamomaFoundation.6.dylib" "@rpath/libJamomaFoundation.6.dylib" "$1/../libAPIJamoma.dylib"
install_name_tool -change "@executable_path/../MacOS/libJamomaModular.6.dylib" "@rpath/libJamomaModular.6.dylib" "$1/../libAPIJamoma.dylib"
install_name_tool -change "@executable_path/../MacOS/libJamomaDSP.6.dylib" "@rpath/libJamomaDSP.6.dylib" "$1/../libAPIJamoma.dylib"
install_name_tool -add_rpath @executable_path/plugins "$1/../i-score"

rm "$1/../libJamomaFoundation.6.dylib" "$1/../libJamomaModular.6.dylib" "$1/../libJamomaDSP.6.dylib" 