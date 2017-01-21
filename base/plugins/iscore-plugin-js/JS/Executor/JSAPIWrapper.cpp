#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include "JSAPIWrapper.hpp"
#include <ossia/editor/value/value.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>

namespace JS
{
QJSValue APIWrapper::value(QJSValue address)
{
  // OPTIMIZEME : have State::Address::fromString return a optional<Address> to
  // have a single check.
  auto addr_str = address.toString();
  auto acc_opt = State::AddressAccessor::fromString(addr_str);
  if (acc_opt)
  {
    // FIXME this does not handle accessors at all
    const State::AddressAccessor& acc = *acc_opt;
    auto val = devices.updateProxy.try_refreshRemoteValue(acc.address);
    if (val)
    {
      return JS::convert::value(m_engine, *std::move(val));
    }
  }

  return {};
}
namespace convert
{
QJSValue makeImpulse(QJSEngine& engine)
{
  auto obj = engine.newObject();
  obj.setProperty("impulse", 1);
  return obj;
}

QJSValue value(QJSEngine& engine, const State::Value& val)
{
  const struct
  {
    QJSEngine& engine;

  public:
    using return_type = QJSValue;
    return_type operator()() const
    {
      return makeImpulse(engine);
    }
    return_type operator()(const State::impulse_t&) const
    {
      return makeImpulse(engine);
    }
    return_type operator()(int i) const
    {
      return i;
    }
    return_type operator()(float f) const
    {
      return f;
    }
    return_type operator()(bool b) const
    {
      return b;
    }
    return_type operator()(const QString& s) const
    {
      return s;
    }
    return_type operator()(const std::string& s) const
    {
      return QString::fromStdString(s);
    }

    return_type operator()(QChar c) const
    {
      // Note : it is saved as a string but the actual type should be saved
      // also
      // so that the QChar can be recovered.
      return QString(c);
    }

    return_type operator()(char c) const
    {
      return QString(QChar(c));
    }

    return_type operator()(const State::vec2f& t) const
    {
      auto arr = engine.newArray(t.size());

      for (auto i = 0U; i < t.size(); i++)
      {
        arr.setProperty(i, t[i]);
      }

      return arr;
    }

    return_type operator()(const State::vec3f& t) const
    {
      auto arr = engine.newArray(t.size());

      for (auto i = 0U; i < t.size(); i++)
      {
        arr.setProperty(i, t[i]);
      }

      return arr;
    }

    return_type operator()(const State::vec4f& t) const
    {
      auto arr = engine.newArray(t.size());

      for (auto i = 0U; i < t.size(); i++)
      {
        arr.setProperty(i, t[i]);
      }

      return arr;
    }

    return_type operator()(const State::tuple_t& t) const
    {
      auto arr = engine.newArray(t.size());

      int i = 0;
      for (const auto& elt : t)
      {
        arr.setProperty(i++, eggs::variants::apply(*this, elt.impl()));
      }

      return arr;
    }
  } visitor{engine};

  return ossia::apply(visitor, val.val.impl());
}

QJSValue address(const State::AddressAccessor& val)
{
  return val.toString();
}

QJSValue message(QJSEngine& engine, const State::Message& mess)
{
  auto obj = engine.newObject();
  auto& strings = iscore::StringConstant();

  obj.setProperty(strings.address, address(mess.address));
  obj.setProperty(strings.value, value(engine, mess.value));
  return obj;
}

QJSValue time(const TimeVal& val)
{
  return val.msec();
}

QJSValue messages(QJSEngine& engine, const State::MessageList& messages)
{
  auto obj = engine.newArray(messages.size());
  for (int i = 0; i < messages.size(); i++)
  {
    obj.setProperty(i, message(engine, messages.at(i)));
  }
  return obj;
}

State::ValueImpl value(const QJSValue& val)
{
  if (val.isUndefined() || val.isNull() || val.isError())
    return {};
  else if (val.isBool())
    return val.toBool();
  else if (val.isNumber())
    return val.toNumber();
  else if (val.isString())
    return val.toString().toStdString();
  else if (val.isArray())
  {
    State::tuple_t arr;

    QJSValueIterator it{val};
    while (it.hasNext())
    {
      it.next();
      arr.push_back(value(it.value()));
    }
    return arr;
  }
  else if (val.isObject())
  {
    if (val.hasProperty("impulse"))
    {
      return State::impulse_t{};
    }
  }

  return {};
}

State::Message message(const QJSValue& val)
{
  auto& strings = iscore::StringConstant();
  if (val.isObject())
  {
    auto iscore_addr = val.property(strings.address);
    auto iscore_val = val.property(strings.value);
    if (iscore_addr.isString())
    {
      auto res = State::AddressAccessor::fromString(iscore_addr.toString());
      if (res)
        return {*res, value(iscore_val)};
      return {};
    }
  }
  else if (val.isString())
  {
    auto iscore_addr = val.property(strings.address);
    auto res = State::AddressAccessor::fromString(iscore_addr.toString());
    if (res)
      return State::Message{*res, State::Value::fromValue(State::impulse_t{})};
  }

  return {};
}

State::MessageList messages(const QJSValue& val)
{
  State::MessageList ml;

  if (val.isArray())
  {
    QJSValueIterator it{val};
    while (it.hasNext())
    {
      it.next();
      auto mess = message(it.value());
      if (!mess.address.address.device.isEmpty())
      {
        ml.append(std::move(mess));
      }
    }
  }
  else if (val.isObject())
  {
    auto mess = message(val);
    if (!mess.address.address.device.isEmpty())
    {
      ml.append(std::move(mess));
    }
  }
  else if (val.isString())
  {
    auto res = State::AddressAccessor::fromString(val.toString());
    if (res)
      ml.append({*res, State::Value::fromValue(State::impulse_t{})});
  }

  return ml;
}
}
}
