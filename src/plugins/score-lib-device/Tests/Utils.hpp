#pragma once

#include <State/Message.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/application/MockApplication.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QMetaType>
#include <QObject>

static auto init = [] {
  // Qt6: QDataStream stream operators and comparators for registered metatypes
  // are picked up automatically (qRegisterMetaTypeStreamOperators /
  // QMetaType::registerComparators were removed).
  static score::MinimalApplication app;
  return 0;
}();
