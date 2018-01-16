#!/bin/zsh
# note: useful snippets:
#  find . -name '*.h' | sed 's/\.\//\/opt\/score-scan-build\//g' | xargs -n 1 dirname | sort | uniq | awk '{ print "    -I" $1 " \\" }'
#  find . -name '*.hpp' | sed 's/\.\//\/opt\/score-scan-build\//g' | xargs -n 1 dirname | sort | uniq | awk '{ print "    -I" $1 " \\" }'
#   ls | grep score | awk '{ print "    -I~/i-score/base/plugins/" $1 " \\" }'

SCORE_ROOT=/opt/score-static

ls $HOME/i-score/API/OSSIA/ossia/**/*.cpp | grep -v phidget | awk '{ print "#include \"" $1  "\""} ' > /tmp/out.cpp
ls $HOME/i-score/API/OSSIA/ossia-c/**/*.cpp | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp
ls $HOME/i-score/API/OSSIA/ossia-qt/**/*.cpp | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp
ls $HOME/i-score/base/lib/**/*.cpp | grep -v Test | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp
ls $HOME/i-score/base/plugins/**/*.cpp | grep -v Test | grep -v ExpressionParser | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp
echo $SCORE_ROOT/**/mocs_compilation.cpp  | sort | uniq | sed 's/ /\n/g' | grep -v addon | while read -r file ; do
  if grep some_compilers $file; then
    ;
  else
    echo "$file" | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp
   fi
done

echo "$HOME/i-score/base/app/Application.cpp" | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp
echo "$HOME/i-score/API/3rdparty/ModernMIDI/ModernMIDI/midi_input.cpp" | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp
echo "$HOME/i-score/API/3rdparty/ModernMIDI/ModernMIDI/midi_output.cpp" | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp
echo "$HOME/i-score/API/3rdparty/ModernMIDI/ModernMIDI/midi_utils.cpp" | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp
echo "$HOME/i-score/API/3rdparty/ModernMIDI/ModernMIDI/music_theory.cpp" | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp
echo "$HOME/i-score/API/3rdparty/ModernMIDI/third_party/rtmidi/RtMidi.cpp" | awk '{ print "#include \"" $1  "\""} ' >> /tmp/out.cpp

# -enable-checker alpha.clone.CloneChecker \ too much false positive for now

#~ scan-build \
    #~ -disable-checker deadcode.DeadStores \
    #~ -enable-checker core.DivideZero \
    #~ -enable-checker core.CallAndMessage \
    #~ -enable-checker core.DynamicTypePropagation \
    #~ -enable-checker alpha.core.BoolAssignment \
    #~ -enable-checker alpha.core.CastSize \
    #~ -enable-checker alpha.core.CastToStruct \
    #~ -enable-checker alpha.core.IdenticalExpr \
    #~ -enable-checker alpha.core.SizeofPtr \
    #~ -enable-checker alpha.core.PointerArithm \
    #~ -enable-checker alpha.core.PointerSub \
    #~ -enable-checker alpha.core.SizeofPtr \
    #~ -enable-checker alpha.core.TestAfterDivZero \
    #~ -enable-checker alpha.cplusplus.IteratorRange \
    #~ -enable-checker alpha.cplusplus.MisusedMovedObject \
    #~ -enable-checker alpha.security.ArrayBoundV2 \
    #~ -enable-checker alpha.security.MallocOverflow \
    #~ -enable-checker alpha.security.ReturnPtrRange \
    #~ -enable-checker alpha.security.taint.TaintPropagation \
    #~ -enable-checker alpha.unix.SimpleStream \
    #~ -enable-checker alpha.unix.cstring.BufferOverlap \
    #~ -enable-checker alpha.unix.cstring.NotNullTerminated \
    #~ -enable-checker alpha.unix.cstring.OutOfBounds \
    #~ -enable-checker alpha.core.FixedAddr \
    #~ -enable-checker optin.performance.Padding \
    #~ -enable-checker security.insecureAPI.strcpy \

    clang++ -O0 -fPIC -std=c++1z \
    -DASIO_STANDALONE=1 -DBOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING \
    -DBOOST_MULTI_INDEX_ENABLE_SAFE_MODE -DQT_CORE_LIB \
    -DQT_DISABLE_DEPRECATED_BEFORE=0x050800 -DQT_GUI_LIB \
    -DQT_NETWORK_LIB -DQT_QML_LIB -DQT_QUICK_LIB -DQT_SERIALPORT_LIB \
    -DQT_WEBSOCKETS_LIB -DQT_WIDGETS_LIB -DRAPIDJSON_HAS_STDSTRING=1 \
    -DSCORE_DEBUG -DSCORE_LIB_BASE -DSCORE_LIB_INSPECTOR \
    -DSCORE_LIB_STATE -DTINYSPLINE_DOUBLE_PRECISION \
    -DHAS_FAUST=1 \
    -DHAS_VST2=1 \
    -DHAS_LV2=1 \
    -DLILV_SHARED=1 \
    -c /tmp/out.cpp \
    "$HOME/i-score/base/plugins/score-lib-state/State/ExpressionParser.cpp" \
    -I/usr/include/qt \
    -I/usr/include/qt/QtCore \
    -I/usr/include/qt/QtGui \
    -I/usr/include/qt/QtWidgets \
    -I/usr/include/qt/QtNetwork \
    -I/usr/include/qt/QtWebSockets \
    -I/usr/include/qt/QtSvg \
    -I/usr/include/qt/QtQml \
    -I/usr/include/qt/QtSerialPort \
    -I/usr/include/qt/QtQuick \
    -I/usr/include/qt/QtQuickControls2 \
    -I/usr/include/qt/QtXml \
    -I/usr/include/lilv-0 \
    -I$HOME/i-score/API/OSSIA \
    -I$HOME/i-score/API/3rdparty/asio/asio/include \
    -I$HOME/i-score/API/3rdparty/flat_hash_map \
    -I$HOME/i-score/API/3rdparty/hopscotch-map/src \
    -I$HOME/i-score/API/3rdparty/SmallFunction/smallfun/include \
    -I$HOME/i-score/API/3rdparty/brigand/include \
    -I$HOME/i-score/API/3rdparty/chobo-shl/include \
    -I$HOME/i-score/API/3rdparty/exprtk \
    -I$HOME/i-score/API/3rdparty/frozen/include \
    -I$HOME/i-score/API/3rdparty/GSL/include \
    -I$HOME/i-score/API/3rdparty/ModernMIDI \
    -I$HOME/i-score/API/3rdparty/readerwriterqueue \
    -I$HOME/i-score/API/3rdparty/fmt \
    -I$HOME/i-score/API/3rdparty/oscpack \
    -I$HOME/i-score/API/3rdparty/nano-signal-slot/include \
    -I$HOME/i-score/API/3rdparty/Servus/ \
    -I$HOME/i-score/API/3rdparty/pybind11/include \
    -I$HOME/i-score/API/3rdparty/rapidjson/include \
    -I$HOME/i-score/API/3rdparty/spdlog/include \
    -I$HOME/i-score/API/3rdparty/variant/include \
    -I$HOME/i-score/API/3rdparty/websocketpp/ \
    -I$HOME/i-score/3rdparty/QProgressIndicator \
    -I$HOME/i-score/3rdparty/QRecentFilesMenu \
    -I$HOME/i-score/3rdparty/Qt-Color-Widgets \
    -I$HOME/i-score/3rdparty/quazip \
    -I$HOME/i-score/base/app \
    -I$HOME/i-score/base/lib \
    -I$HOME/i-score/base/addons/iscore-addon-pd \
    -I$HOME/i-score/base/addons/iscore-addon-shaders \
    -I$HOME/i-score/base/addons/iscore-addon-shaders/3rdparty/libisf \
    -I$HOME/i-score/base/plugins/ \
    -I$HOME/i-score/base/plugins/score-lib-device \
    -I$HOME/i-score/base/plugins/score-lib-inspector \
    -I$HOME/i-score/base/plugins/score-lib-process \
    -I$HOME/i-score/base/plugins/score-lib-state \
    -I$HOME/i-score/base/plugins/score-plugin-automation \
    -I$HOME/i-score/base/plugins/score-plugin-curve \
    -I$HOME/i-score/base/plugins/score-plugin-deviceexplorer \
    -I$HOME/i-score/base/plugins/score-plugin-engine \
    -I$HOME/i-score/base/plugins/score-plugin-fx \
    -I$HOME/i-score/base/plugins/score-plugin-inspector \
    -I$HOME/i-score/base/plugins/score-plugin-js \
    -I$HOME/i-score/base/plugins/score-plugin-library \
    -I$HOME/i-score/base/plugins/score-plugin-loop \
    -I$HOME/i-score/base/plugins/score-plugin-mapping \
    -I$HOME/i-score/base/plugins/score-plugin-media \
    -I$HOME/i-score/base/plugins/score-plugin-midi \
    -I$HOME/i-score/base/plugins/score-plugin-pluginsettings \
    -I$HOME/i-score/base/plugins/score-plugin-recording \
    -I$HOME/i-score/base/plugins/score-plugin-scenario \
    -I$SCORE_ROOT/ \
    -I$SCORE_ROOT/API/OSSIA \
    -I$SCORE_ROOT/base/addons/iscore-addon-pd \
    -I$SCORE_ROOT/base/addons/iscore-addon-shaders \
    -I$SCORE_ROOT/base/plugins/score-plugin-automation \
    -I$SCORE_ROOT/base/plugins/score-plugin-curve \
    -I$SCORE_ROOT/base/plugins/score-plugin-deviceexplorer \
    -I$SCORE_ROOT/base/plugins/score-plugin-js \
    -I$SCORE_ROOT/base/plugins/score-plugin-loop \
    -I$SCORE_ROOT/base/plugins/score-plugin-mapping \
    -I$SCORE_ROOT/base/plugins/score-plugin-media \
    -I$SCORE_ROOT/base/plugins/score-plugin-midi \
    -I$SCORE_ROOT/base/plugins/score-plugin-recording \
    -I$SCORE_ROOT/base/plugins/score-plugin-scenario \
    -I$SCORE_ROOT/3rdparty/QProgressIndicator/QProgressIndicator_autogen \
    -I$SCORE_ROOT/3rdparty/QRecentFilesMenu/QRecentFilesMenu_autogen \
    -I$SCORE_ROOT/3rdparty/Qt-Color-Widgets \
    -I$SCORE_ROOT/3rdparty/Qt-Color-Widgets/ColorWidgets-qt5_autogen \
    -I$SCORE_ROOT/3rdparty/quazip/quazip/quazip_autogen \
    -I$SCORE_ROOT/3rdparty/zlib \
    -I$SCORE_ROOT/3rdparty/zlib/zlibstatic_autogen \
    -I$SCORE_ROOT/API/3rdparty/ModernMIDI/ModernMIDI_autogen \
    -I$SCORE_ROOT/API/3rdparty/Servus/servus \
    -I$SCORE_ROOT/API/3rdparty/Servus/servus/Servus_autogen \
    -I$SCORE_ROOT/API/3rdparty/Servus/servus/ServusQt_autogen \
    -I$SCORE_ROOT/API/OSSIA \
    -I$SCORE_ROOT/API/OSSIA/ossia_autogen \
    -I$SCORE_ROOT/base/addons/iscore-addon-pd \
    -I$SCORE_ROOT/base/addons/iscore-addon-pd/score_addon_pd_autogen \
    -I$SCORE_ROOT/base/addons/iscore-addon-pd/Tests/Pd_DataflowTest_autogen \
    -I$SCORE_ROOT/base/addons/iscore-addon-shaders \
    -I$SCORE_ROOT/base/addons/iscore-addon-shaders/score_addon_shader_autogen \
    -I$SCORE_ROOT/base/addons/iscore-addon-staticanalysis \
    -I$SCORE_ROOT/base/addons/iscore-addon-staticanalysis/score_addon_staticanalysis_autogen \
    -I$SCORE_ROOT/base/addons/score-plugin-carrousel \
    -I$SCORE_ROOT/base/addons/score-plugin-carrousel/score_plugin_carrousel_autogen \
    -I$SCORE_ROOT/base/app/score_autogen \
    -I$SCORE_ROOT/base/lib \
    -I$SCORE_ROOT/base/lib/score_lib_base_autogen \
    -I$SCORE_ROOT/base/plugins/score-lib-device \
    -I$SCORE_ROOT/base/plugins/score-lib-device/score_lib_device_autogen \
    -I$SCORE_ROOT/base/plugins/score-lib-inspector \
    -I$SCORE_ROOT/base/plugins/score-lib-inspector/score_lib_inspector_autogen \
    -I$SCORE_ROOT/base/plugins/score-lib-process \
    -I$SCORE_ROOT/base/plugins/score-lib-process/score_lib_process_autogen \
    -I$SCORE_ROOT/base/plugins/score-lib-process/Tests/Process_PortSerializationTest_autogen \
    -I$SCORE_ROOT/base/plugins/score-lib-state \
    -I$SCORE_ROOT/base/plugins/score-lib-state/score_lib_state_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-automation \
    -I$SCORE_ROOT/base/plugins/score-plugin-automation/score_plugin_automation_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-curve \
    -I$SCORE_ROOT/base/plugins/score-plugin-curve/score_plugin_curve_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-deviceexplorer \
    -I$SCORE_ROOT/base/plugins/score-plugin-deviceexplorer/score_plugin_deviceexplorer_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-engine \
    -I$SCORE_ROOT/base/plugins/score-plugin-engine/score_plugin_engine_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-fx \
    -I$SCORE_ROOT/base/plugins/score-plugin-fx/score_plugin_fx_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-inspector \
    -I$SCORE_ROOT/base/plugins/score-plugin-inspector/score_plugin_inspector_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-js \
    -I$SCORE_ROOT/base/plugins/score-plugin-js/score_plugin_js_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-library \
    -I$SCORE_ROOT/base/plugins/score-plugin-library/score_plugin_library_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-loop \
    -I$SCORE_ROOT/base/plugins/score-plugin-loop/score_plugin_loop_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-mapping \
    -I$SCORE_ROOT/base/plugins/score-plugin-mapping/score_plugin_mapping_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-media \
    -I$SCORE_ROOT/base/plugins/score-plugin-media/score_plugin_media_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-midi \
    -I$SCORE_ROOT/base/plugins/score-plugin-midi/score_plugin_midi_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-pluginsettings \
    -I$SCORE_ROOT/base/plugins/score-plugin-pluginsettings/score_plugin_pluginsettings_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-recording \
    -I$SCORE_ROOT/base/plugins/score-plugin-recording/score_plugin_recording_autogen \
    -I$SCORE_ROOT/base/plugins/score-plugin-scenario \
    -I$SCORE_ROOT/base/plugins/score-plugin-scenario/score_plugin_scenario_autogen


clang++ -fuse-ld=lld ExpressionParser.o out.o -lQt5Core -lQt5Gui -lQt5Widgets -lQt5Qml -lQt5Quick -lQt5Network -lQt5WebSockets -lavcodec -lavdevice -lavfilter -lavformat -lavresample


