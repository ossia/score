#include "AutomationState.hpp"
#include "Automation/AutomationModel.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"

AutomationState::AutomationState(const AutomationModel* model, double watchedPoint):
    ProcessStateDataInterface{model},
    m_point{watchedPoint}
{
    Q_ASSERT(0 <= watchedPoint && watchedPoint <= 1);

    connect(model, &AutomationModel::curveChanged,
            this, &ProcessStateDataInterface::stateChanged);

    connect(model, &AutomationModel::addressChanged,
            this, &ProcessStateDataInterface::stateChanged);
}

iscore::Message AutomationState::message() const
{
    iscore::Message m;
    m.address = model()->address();
    for(const CurveSegmentModel& seg : model()->curve().segments())
    {
        if(seg.start().x() <= m_point && seg.end().x() >= m_point)
        {
            m.value.val = seg.valueAt(m_point);
            break;
        }
    }

    return m;
}

AutomationState *AutomationState::clone() const
{
    return new AutomationState{model(), m_point};
}

const AutomationModel* AutomationState::model() const
{
    return static_cast<const AutomationModel*>(ProcessStateDataInterface::model());
}
