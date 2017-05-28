#pragma once
#include <QMetaType>

namespace Scenario
{
enum class ConstraintExecutionState: uint8_t
{
  Enabled,
  Disabled,
  Muted
};
}
Q_DECLARE_METATYPE(Scenario::ConstraintExecutionState)
