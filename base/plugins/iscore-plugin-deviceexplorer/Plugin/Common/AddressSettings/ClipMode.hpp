#pragma once
#include <QMap>
namespace iscore {
enum class ClipMode : int { Clip, Fold, Free, Wrap };
const QMap<ClipMode, QString> &ClipModeStringMap();
}
