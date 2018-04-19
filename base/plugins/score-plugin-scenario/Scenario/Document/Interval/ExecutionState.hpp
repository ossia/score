#pragma once
#include <QMetaType>

namespace Scenario
{
enum class IntervalExecutionState : uint8_t
{
  Enabled,
  Disabled,
  Muted
};
}
Q_DECLARE_METATYPE(Scenario::IntervalExecutionState)
