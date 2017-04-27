#pragma once
#include <iscore_lib_device_export.h>
#include <ossia/network/common/address_properties.hpp>
#include <iscore/tools/std/Optional.hpp>

class QString;
template <class Key, class T>
class QMap;

namespace Device
{
ISCORE_LIB_DEVICE_EXPORT const QMap<ossia::access_mode, QString>& AccessModeText();

inline bool hasInput(const optional<ossia::access_mode>& t)
{
  return t && (*t == ossia::access_mode::BI || *t == ossia::access_mode::GET);
}
inline bool hasOutput(const optional<ossia::access_mode>& t)
{
  return t && (*t == ossia::access_mode::BI || *t == ossia::access_mode::SET);
}
}
