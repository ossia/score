#pragma once

#include <verdigris>
namespace Scenario
{
enum class IntervalExecutionState : uint8_t
{
  Enabled = 0,
  Disabled = 1,
  Muted = 2
};
enum class IntervalExecutionEvent : uint8_t
{
  Enabled,
  Disabled,
  Muted,
  Unmuted,
  Playing,
  Stopped,
  Paused,
  Resumed,
  Finished
};
}
Q_DECLARE_METATYPE(Scenario::IntervalExecutionState)
W_REGISTER_ARGTYPE(Scenario::IntervalExecutionState)

Q_DECLARE_METATYPE(Scenario::IntervalExecutionEvent)
W_REGISTER_ARGTYPE(Scenario::IntervalExecutionEvent)
