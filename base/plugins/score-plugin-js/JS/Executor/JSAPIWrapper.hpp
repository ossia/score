#pragma once
#include <Process/TimeValue.hpp>
#include <QChar>
#include <QObject>
#include <QString>
#include <QtQml/QJSEngine>
#include <QtQml/QJSValue>
#include <QtQml/QJSValueIterator>
#include <State/Message.hpp>
#include <eggs/variant/variant.hpp>

#include <State/Address.hpp>
#include <State/Value.hpp>

// TODO cleanup this file

namespace Explorer
{
class DeviceDocumentPlugin;
}

namespace JS
{
namespace convert
{
QJSValue makeImpulse(QJSEngine& engine);

QJSValue value(QJSEngine& engine, const ossia::value& val);

QJSValue address(const State::AddressAccessor& val);

QJSValue message(QJSEngine& engine, const State::Message& mess);

QJSValue time(const TimeVal& val);

// TODO vector instead of MessageList.
QJSValue messages(QJSEngine& engine, const State::MessageList& messages);

ossia::value value(const QJSValue& val);
State::Message message(const QJSValue& val);
State::MessageList messages(const QJSValue& val);
}

class APIWrapper : public QObject
{
  Q_OBJECT
public:
  APIWrapper(QJSEngine& engine, const Explorer::DeviceDocumentPlugin& devs)
      : m_engine{engine}, devices{devs}
  {
  }

public Q_SLOTS:
  QJSValue value(QJSValue address);
  QJSValue clone(QJSValue address);

private:
  QJSEngine& m_engine;
  const Explorer::DeviceDocumentPlugin& devices;
};
}
