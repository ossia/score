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
}
Q_DECLARE_METATYPE(Scenario::IntervalExecutionState)
W_REGISTER_ARGTYPE(Scenario::IntervalExecutionState)
