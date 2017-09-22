#pragma once
#include <score_lib_device_export.h>
#include <ossia/network/common/parameter_properties.hpp>
#include <score/tools/std/Optional.hpp>

class QString;
template <class Key, class T>
class QMap;

namespace Device
{
SCORE_LIB_DEVICE_EXPORT const QMap<ossia::access_mode, QString>& AccessModeText();

inline bool hasInput(const optional<ossia::access_mode>& t)
{
  return t && (*t == ossia::access_mode::BI || *t == ossia::access_mode::GET);
}
inline bool hasOutput(const optional<ossia::access_mode>& t)
{
  return t && (*t == ossia::access_mode::BI || *t == ossia::access_mode::SET);
}
}
