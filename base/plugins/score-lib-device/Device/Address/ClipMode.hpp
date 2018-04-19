#pragma once
#include <ossia/network/common/parameter_properties.hpp>

#include <cstdint>
#include <score_lib_device_export.h>

class QString;
template <class Key, class T>
class QMap;

namespace Device
{

SCORE_LIB_DEVICE_EXPORT const QMap<ossia::bounding_mode, QString>&
ClipModeStringMap();
SCORE_LIB_DEVICE_EXPORT const QMap<ossia::bounding_mode, QString>&
ClipModePrettyStringMap();
}
