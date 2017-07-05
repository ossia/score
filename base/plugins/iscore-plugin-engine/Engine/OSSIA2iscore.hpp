#pragma once
#include <ossia/network/common/address_properties.hpp>
#include <ossia/network/domain/domain_fwd.hpp>
#include <ossia/network/base/device.hpp>
#include <Process/TimeValue.hpp>

#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/value/vec.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <State/Value.hpp>
#include <iscore_plugin_engine_export.h>

namespace ossia
{
class value;
namespace net
{
class node_base;
}
}

namespace Device
{
class DeviceList;
}
// Utility functions to convert from one node to another.
namespace Engine
{
namespace ossia_to_iscore
{

template <typename>
struct MatchingType;
template <>
struct MatchingType<float>
{
  static constexpr const auto val = ossia::val_type::FLOAT;
  using type = float;
  static auto convert(float f)
  {
    return State::Value::fromValue(f);
  }
};
template <>
struct MatchingType<double>
{
  static constexpr const auto val = ossia::val_type::FLOAT;
  using type = float;
  static auto convert(double f)
  {
    return State::Value::fromValue(f);
  }
};
template <>
struct MatchingType<int>
{
  static constexpr const auto val = ossia::val_type::INT;
  using type = int32_t;
  static auto convert(int f)
  {
    return State::Value::fromValue(f);
  }
};
template <>
struct MatchingType<bool>
{
  static constexpr const auto val = ossia::val_type::BOOL;
  using type = bool;
  static auto convert(bool f)
  {
    return State::Value::fromValue(f);
  }
};
template <>
struct MatchingType<State::impulse>
{
  static constexpr const auto val = ossia::val_type::IMPULSE;
  using type = ossia::impulse;
  static auto convert(State::impulse)
  {
    return State::Value::fromValue(State::impulse{});
  }
};
template <>
struct MatchingType<std::string>
{
  static constexpr const auto val = ossia::val_type::STRING;
  using type = std::string;
  static auto convert(const std::string& f)
  {
    return State::Value::fromValue(f);
  }
  static auto convert(std::string&& f)
  {
    return State::Value::fromValue(std::move(f));
  }
};
template <>
struct MatchingType<QString>
{
  static constexpr const auto val = ossia::val_type::STRING;
  using type = std::string;
  static auto convert(const QString& f)
  {
    return State::Value::fromValue(f.toStdString());
  }
  static auto convert(QString&& f)
  {
    return State::Value::fromValue(f.toStdString());
  }
};
template <>
struct MatchingType<char>
{
  static constexpr const auto val = ossia::val_type::CHAR;
  using type = char;
  static auto convert(char f)
  {
    return State::Value::fromValue(f);
  }
};
template <>
struct MatchingType<QChar>
{
  static constexpr const auto val = ossia::val_type::CHAR;
  using type = char;
  static auto convert(QChar f)
  {
    return State::Value::fromValue(f.toLatin1());
  }
};
template <>
struct MatchingType<State::vec2f>
{
  static constexpr const auto val = ossia::val_type::VEC2F;
  using type = ossia::vec2f;
  static auto convert(const State::vec2f& t)
  {
    return State::Value::fromValue(t);
  }
};
template <>
struct MatchingType<State::vec3f>
{
  static constexpr const auto val = ossia::val_type::VEC3F;
  using type = ossia::vec3f;
  static auto convert(const State::vec3f& t)
  {
    return State::Value::fromValue(t);
  }
};
template <>
struct MatchingType<State::vec4f>
{
  static constexpr const auto val = ossia::val_type::VEC4F;
  using type = ossia::vec4f;
  static auto convert(const State::vec4f& t)
  {
    return State::Value::fromValue(t);
  }
};
template <>
struct MatchingType<State::tuple_t>
{
  static constexpr const auto val = ossia::val_type::TUPLE;
  using type = std::vector<ossia::value>;
  static auto convert(const State::tuple_t& t)
  {
    return State::Value::fromValue(t);
  }
  static auto convert(State::tuple_t&& t)
  {
    return State::Value::fromValue(std::move(t));
  }
};
template <>
struct MatchingType<::TimeVal>
{
  static constexpr const auto val = ossia::val_type::FLOAT;
  using type = float;
  static auto convert(::TimeVal&& t)
  {
    return State::Value::fromValue(t.msec());
  }
};

ISCORE_PLUGIN_ENGINE_EXPORT ossia::bounding_mode
ToClipMode(ossia::bounding_mode b);

ISCORE_PLUGIN_ENGINE_EXPORT State::Value ToValue(ossia::val_type);

ISCORE_PLUGIN_ENGINE_EXPORT State::Address
ToAddress(const ossia::net::node_base& node);
ISCORE_PLUGIN_ENGINE_EXPORT Device::AddressSettings
ToAddressSettings(const ossia::net::node_base& node);
ISCORE_PLUGIN_ENGINE_EXPORT Device::FullAddressSettings
ToFullAddressSettings(const ossia::net::node_base& node);

ISCORE_PLUGIN_ENGINE_EXPORT Device::FullAddressSettings
ToFullAddressSettings(const State::Address& addr, const Device::DeviceList&);

ISCORE_PLUGIN_ENGINE_EXPORT Device::Node
ToDeviceExplorer(const ossia::net::node_base& node);

inline ::TimeVal defaultTime(ossia::time_value t)
{
  return t.infinite() ? ::TimeVal{PositiveInfinity{}}
                        : ::TimeVal::fromMsecs(double(t) / 1000.);
}
}
}
