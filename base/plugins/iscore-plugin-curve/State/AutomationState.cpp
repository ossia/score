#include "AutomationState.hpp"
#include "Automation/AutomationModel.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"

AutomationState::AutomationState(
        const AutomationModel& model,
        double watchedPoint,
        QObject* parent):
    ProcessStateDataInterface{model, parent},
    m_point{watchedPoint}
{
    ISCORE_ASSERT(0 <= watchedPoint && watchedPoint <= 1);

    con(this->model(), &AutomationModel::curveChanged,
            this, &DynamicStateDataInterface::stateChanged);

    con(this->model(), &AutomationModel::addressChanged,
            this, &DynamicStateDataInterface::stateChanged);
}

iscore::Message AutomationState::message() const
{
    iscore::Message m;
    m.address = model().address();
    for(const CurveSegmentModel& seg : model().curve().segments())
    {
        if(seg.start().x() <= m_point && seg.end().x() >= m_point)
        {
            m.value.val = seg.valueAt(m_point);
            break;
        }
    }

    return m;
}

AutomationState *AutomationState::clone(QObject* parent) const
{
    return new AutomationState{model(), m_point, parent};
}

const AutomationModel& AutomationState::model() const
{
    return static_cast<const AutomationModel&>(ProcessStateDataInterface::model());
}
