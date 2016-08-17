#pragma once
#include <iscore_lib_device_export.h>
class QString;
template <class Key, class T> class QMap;

namespace Device {
enum class ClipMode : int { Clip, Fold, Free, Wrap, Low, High };

ISCORE_LIB_DEVICE_EXPORT const QMap<ClipMode, QString> &ClipModeStringMap();
ISCORE_LIB_DEVICE_EXPORT const QMap<ClipMode, QString> &ClipModePrettyStringMap();
}
