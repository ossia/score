#pragma once
#include <State/Address.hpp>

#include <Process/TimeValue.hpp>

#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/network/value/value.hpp>

#include <ossia-qt/matching_type.hpp>
namespace ossia
{
template <>
struct qt_property_converter<::TimeVal>
{
  static constexpr const auto val = ossia::val_type::FLOAT;
  using type = float;
  static auto convert(const TimeVal& t) { return t.msec(); }
};
}

namespace ossia
{
template <>
struct qt_property_converter<State::AddressAccessor>
{
  static constexpr const auto val = ossia::val_type::STRING;
  using type = std::string;
  static std::string convert(const State::AddressAccessor& t)
  {
    return t.toString().toStdString();
  }
};

template <>
struct qt_property_converter<ossia::value>
{
  static constexpr const auto val = ossia::val_type::LIST;
  using type = ossia::value;
  static ossia::value convert(const ossia::value& t) { return t; }
  static ossia::value convert(ossia::value&& t) { return std::move(t); }
};
}
