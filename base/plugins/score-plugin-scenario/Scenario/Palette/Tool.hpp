#pragma once
#include <QMetaType>

namespace Scenario
{
enum class Tool: int8_t
{
  Disabled,
  Create,
  Select,
  Play,
  MoveSlot,
  Playing
};
}
Q_DECLARE_METATYPE(Scenario::Tool)
