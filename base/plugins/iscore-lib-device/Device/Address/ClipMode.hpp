#pragma once
#include <iscore_lib_device_export.h>
#include <ossia/network/common/address_properties.hpp>
#include <cstdint>

class QString;
template <class Key, class T>
class QMap;

namespace Device
{

ISCORE_LIB_DEVICE_EXPORT const QMap<ossia::bounding_mode, QString>& ClipModeStringMap();
ISCORE_LIB_DEVICE_EXPORT const QMap<ossia::bounding_mode, QString>&
ClipModePrettyStringMap();
}
