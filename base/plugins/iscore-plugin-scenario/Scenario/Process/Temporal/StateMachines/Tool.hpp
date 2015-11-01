#pragma once
#include <QMetaType>

enum class ScenarioToolKind { Create, Select, MoveSlot, Play };
Q_DECLARE_METATYPE(ScenarioToolKind)
