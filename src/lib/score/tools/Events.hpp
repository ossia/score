#pragma once
#include <score/config.hpp>

#include <QEvent>
#include <QWidget>

#include <verdigris>

W_REGISTER_ARGTYPE(QEvent*)
W_REGISTER_ARGTYPE(QTabletEvent*)

W_REGISTER_ARGTYPE(QKeyEvent*)
W_REGISTER_ARGTYPE(QMouseEvent*)
W_REGISTER_ARGTYPE(QHoverEvent*)
