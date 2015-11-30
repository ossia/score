#pragma once
class QString;
template <class Key, class T> class QMap;

namespace iscore {
enum class ClipMode : int { Clip, Fold, Free, Wrap };
const QMap<ClipMode, QString> &ClipModeStringMap();
}
