#pragma once
#include <Process/TimeValue.hpp>

#include <ossia-qt/matching_type.hpp>
#include <ossia/editor/scenario/time_value.hpp>
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
