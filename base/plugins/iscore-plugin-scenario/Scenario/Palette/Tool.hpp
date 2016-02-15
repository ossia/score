#pragma once
#include <QMetaType>

namespace Scenario{
enum class Tool { Disabled, Create, Select, Play, MoveSlot, Playing };
}
Q_DECLARE_METATYPE(Scenario::Tool)
