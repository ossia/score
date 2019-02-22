TARGET = i-score
TEMPLATE = app
CONFIG += c++1z object_parallel_to_source warn_off debug

QT+=core widgets gui network xml svg websockets quick
DEFINES += __EMSCRIPTEN__
# cp *.h *.hpp from cmake
INCLUDEPATH += $$PWD/build-debug $$PWD/base/lib \
$$PWD/API/3rdparty/asio/asio/include \
$$PWD/API/3rdparty/brigand/include \
$$PWD/API/3rdparty/chobo-shl/include \
$$PWD/API/3rdparty/fmt \
$$PWD/API/3rdparty/hopscotch-map/src \
$$PWD/API/3rdparty/nano-signal-slot/include \
$$PWD/API/3rdparty/rapidjson/include \
$$PWD/API/3rdparty/flat/include \
$$PWD/API/3rdparty/multi_index/include \
$$PWD/API/3rdparty/readerwriterqueue \
$$PWD/API/3rdparty/concurrentqueue \
$$PWD/API/3rdparty/Servus \
$$PWD/API/3rdparty/spdlog/include \
$$PWD/API/3rdparty/variant/include \
$$PWD/API/3rdparty/websocketpp \
$$PWD/API/3rdparty/GSL/include \
$$PWD/API/3rdparty/bitset2 \
$$PWD/API/3rdparty/RtMidi17 \
$$PWD/API/3rdparty/flat_hash_map \
$$PWD/API/3rdparty/verdigris/src \
$$PWD/API/3rdparty/SmallFunction/smallfun/include \
$$PWD/base/lib/3rdparty/QProgressIndicator \
$$PWD/base/lib/3rdparty/Qt-Color-Widgets \
$$PWD/base/lib/3rdparty/Qt-Color-Widgets/QtColorWidgets \
$$PWD/API/OSSIA \
$$PWD/API/3rdparty/spdlog/include \
$$PWD/API/3rdparty/oscpack \
$$PWD/API/3rdparty/ModernMIDI \
$$PWD/API/3rdparty/rapidjson/include \
$$PWD/API/3rdparty/ModernMIDI/third_party \
$$PWD/base/plugins/score-lib-state \
$$PWD/base/plugins/score-plugin-curve \
$$PWD/base/plugins/score-plugin-deviceexplorer \
$$PWD/base/plugins/score-plugin-inspector \
$$PWD/base/plugins/score-plugin-library \
$$PWD/base/plugins/score-plugin-pluginsettings \
$$PWD/base/plugins/score-plugin-scenario \
$$PWD/base/plugins/score-lib-device \
$$PWD/base/plugins/score-lib-process \
$$PWD/base/plugins/score-plugin-automation \
$$PWD/base/plugins/score-plugin-js \
$$PWD/base/plugins/score-plugin-fx \
$$PWD/base/plugins/score-plugin-mapping \
$$PWD/base/plugins/score-plugin-media \
$$PWD/base/plugins/score-plugin-loop \
$$PWD/base/plugins/score-lib-inspector \
$$PWD/base/plugins/score-plugin-engine \
$$PWD/base/plugins/score-plugin-midi \
$$PWD/base/plugins/score-plugin-recording \
$$PWD/base/plugins/score-plugin-interpolation

DEFINES += QT_STATICPLUGIN SCORE_STATIC_PLUGINS SCORE_DEPLOYMENT_BUILD TINYSPLINE_DOUBLE_PRECISION

include($$PWD/score-srcs.pri)


SOURCES += \
    $$PWD/base/app/main.cpp \
    $$PWD/base/app/Application.cpp

HEADERS += \
    $$PWD/base/app/Application.hpp

INCLUDEPATH += /opt/boost_1_67_0
