#include "AutomationState.hpp"
#include "Automation/AutomationModel.hpp"

AutomationState::AutomationState(const AutomationModel* model, double watchedPoint):
    ProcessStateDataInterface{model},
    m_point{watchedPoint}
{
    Q_ASSERT(0 <= watchedPoint && watchedPoint <= 1);

    connect(model, &AutomationModel::pointsChanged,
            this, &ProcessStateDataInterface::stateChanged);

    connect(model, &AutomationModel::addressChanged,
            this, &ProcessStateDataInterface::stateChanged);
}

Message AutomationState::message() const
{
    Message m;
    m.address = model()->address();
    m.value = model()->points().value(m_point);

    return m;
}

const AutomationModel* AutomationState::model() const
{
    return static_cast<const AutomationModel*>(ProcessStateDataInterface::model());
}
