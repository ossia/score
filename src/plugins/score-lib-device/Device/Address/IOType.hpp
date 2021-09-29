#pragma once
#include <score/tools/std/Optional.hpp>

#include <ossia/network/common/parameter_properties.hpp>
#include <ossia/detail/enum_map.hpp>

#include <score_lib_device_export.h>


class QString;

namespace Device
{
//! To save / reload in JSON
SCORE_LIB_DEVICE_EXPORT
const ossia::enum_map<ossia::access_mode, QString, 3>&
AccessModeText();

//! To show to the user
SCORE_LIB_DEVICE_EXPORT
const ossia::enum_map<ossia::access_mode, QString, 3>&
AccessModePrettyText();

inline bool hasInput(const std::optional<ossia::access_mode>& t)
{
  return t && (*t == ossia::access_mode::BI || *t == ossia::access_mode::GET);
}
inline bool hasOutput(const std::optional<ossia::access_mode>& t)
{
  return t && (*t == ossia::access_mode::BI || *t == ossia::access_mode::SET);
}
}
