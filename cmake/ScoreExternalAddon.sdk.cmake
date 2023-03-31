

foreach(_lib ${SCORE_PLUGINS})
  string(TOLOWER "${_lib}" _lib_lc)
  add_library(score_${_lib_lc} INTERFACE)
  target_compile_definitions(score_${_lib_lc} INTERFACE SCORE_${_lib})
endforeach()

target_compile_options(score_lib_base INTERFACE
  -std=c++2a
  -fPIC
  "SHELL:-Xclang -internal-isystem -Xclang ${SCORE_SDK}/include/x86_64-unknown-linux-gnu/c++/v1"
  "SHELL:-Xclang -internal-isystem -Xclang ${SCORE_SDK}/include/c++/v1/"
  "SHELL:-Xclang -internal-isystem -Xclang ${SCORE_SDK}/include"
  "SHELL:-Xclang -internal-isystem -Xclang ${SCORE_SDK}/lib/clang/16/include"
  "SHELL:-resource-dir ${SCORE_SDK}/lib/clang/16/include"
   -nostdinc
   -nostdlib
)
target_include_directories(score_lib_base SYSTEM INTERFACE
  "${SCORE_SDK}/include/score"
  "${SCORE_SDK}/include/qt"
  "${SCORE_SDK}/include/qt/QtCore"
  "${SCORE_SDK}/include/qt/QtCore/6.5.0"
  "${SCORE_SDK}/include/qt/QtCore/6.5.0/QtCore"
  "${SCORE_SDK}/include/qt/QtCore/6.5.0/QtCore/private"
  "${SCORE_SDK}/include/qt/QtGui"
  "${SCORE_SDK}/include/qt/QtGui/6.5.0"
  "${SCORE_SDK}/include/qt/QtGui/6.5.0/QtGui"
  "${SCORE_SDK}/include/qt/QtGui/6.5.0/QtGui/private"
  "${SCORE_SDK}/include/qt/QtWidgets"
  "${SCORE_SDK}/include/qt/QtWidgets/6.5.0"
  "${SCORE_SDK}/include/qt/QtWidgets/6.5.0/QtWidgets"
  "${SCORE_SDK}/include/qt/QtWidgets/6.5.0/QtWidgets/private"
)

target_compile_definitions(score_lib_base INTERFACE
  BOOST_MATH_DISABLE_FLOAT128=1
  BOOST_ASIO_DISABLE_CONCEPTS=1
  BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
  BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
  QT_CORE_LIB
  QT_DISABLE_DEPRECATED_BEFORE=0x050800
  QT_GUI_LIB
  QT_NETWORK_LIB
  QT_NO_KEYWORDS
  QT_QML_LIB
  QT_QUICK_LIB
  QT_SERIALPORT_LIB
  QT_STATICPLUGIN
  QT_SVG_LIB
  QT_WEBSOCKETS_LIB
  QT_WIDGETS_LIB
  QT_XML_LIB
  RAPIDJSON_HAS_STDSTRING=1
  # SCORE_DEBUG
  TINYSPLINE_DOUBLE_PRECISION
)

function(setup_score_plugin)
endfunction()
