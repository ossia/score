#pragma once

#include <State/Message.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/application/MockApplication.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QMetaType>
#include <QObject>

static auto init = [] {
  static score::MinimalApplication app;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  qRegisterMetaTypeStreamOperators<State::Address>();

  QMetaType::registerComparators<State::MessageList>();
  qRegisterMetaTypeStreamOperators<State::Message>();
  qRegisterMetaTypeStreamOperators<State::MessageList>();
  qRegisterMetaTypeStreamOperators<ossia::value>();
#endif
  return 0;
}();
