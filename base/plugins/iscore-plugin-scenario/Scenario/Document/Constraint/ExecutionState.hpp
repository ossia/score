#pragma once
#include <QMetaType>

namespace Scenario
{
enum class ConstraintExecutionState
{
  Enabled,
  Disabled,
  Muted
};
}
Q_DECLARE_METATYPE(Scenario::ConstraintExecutionState)
