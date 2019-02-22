#pragma once
#include <QMetaType>

#include <wobjectdefs.h>
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
W_REGISTER_ARGTYPE(Scenario::IntervalExecutionState)
