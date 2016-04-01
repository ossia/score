#pragma once
#include <QMetaType>

namespace Scenario
{
enum class ConstraintExecutionState {
    Enabled, Disabled
};
}
Q_DECLARE_METATYPE(Scenario::ConstraintExecutionState)
