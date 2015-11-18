#pragma once
#include <QMetaType>

namespace Scenario{
enum class Tool { Disabled, Create, Select, MoveSlot, Playing };
}
Q_DECLARE_METATYPE(Scenario::Tool)
