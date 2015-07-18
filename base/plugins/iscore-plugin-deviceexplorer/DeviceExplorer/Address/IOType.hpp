#pragma once
#include <QMap>

namespace iscore {
enum class IOType : int { Invalid, In, Out, InOut };
const QMap<IOType, QString>& IOTypeStringMap();
}
