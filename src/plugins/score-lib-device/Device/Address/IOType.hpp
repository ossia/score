#pragma once
#include <score/tools/std/Optional.hpp>

#include <ossia/network/common/parameter_properties.hpp>

#include <score_lib_device_export.h>

class QString;
template <class Key, class T>
class QMap;

namespace Device
{
SCORE_LIB_DEVICE_EXPORT const QMap<ossia::access_mode, QString>& AccessModeText();

inline bool hasInput(const std::optional<ossia::access_mode>& t)
{
  return t && (*t == ossia::access_mode::BI || *t == ossia::access_mode::GET);
}
inline bool hasOutput(const std::optional<ossia::access_mode>& t)
{
  return t && (*t == ossia::access_mode::BI || *t == ossia::access_mode::SET);
}
}
