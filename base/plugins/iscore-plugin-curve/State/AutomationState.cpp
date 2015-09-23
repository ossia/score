#include "AutomationState.hpp"
#include "Automation/AutomationModel.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"

#include "Curve/Segment/Linear/LinearCurveSegmentModel.hpp"
#include "Curve/Segment/Power/PowerCurveSegmentModel.hpp"
AutomationState::AutomationState(
        AutomationModel& model,
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

QString AutomationState::stateName() const
{ return "AutomationState"; }

iscore::Message AutomationState::message() const
{
    iscore::Message m;
    m.address = model().address();
    for(const CurveSegmentModel& seg : model().curve().segments())
    {
        // OPTIMIZEME introduce another index on that has an ordering on curve segments
        // to make this fast (just checking for the first and the last).
        if(seg.start().x() <= m_point && seg.end().x() >= m_point)
        {
            m.value.val = seg.valueAt(m_point) * (model().max() - model().min()) + model().min();
            break;
        }
    }

    return m;
}

double AutomationState::point() const
{ return m_point; }

AutomationState *AutomationState::clone(QObject* parent) const
{
    return new AutomationState{model(), m_point, parent};
}

AutomationModel& AutomationState::model() const
{
    return static_cast<AutomationModel&>(ProcessStateDataInterface::model());
}

std::vector<iscore::Address> AutomationState::matchingAddresses()
{
    // TODO have a better check of "adress validity"
    if(!model().address().device.isEmpty())
        return {model().address()};
    return {};
}

iscore::MessageList AutomationState::messages() const
{
    if(!model().address().device.isEmpty())
        return {message()};
    return {};
}

void AutomationState::setMessages(const iscore::MessageList& received)
{
    if(m_point != 0 && m_point != 1)
        return;

    const auto& segs = model().curve().segments();
    for(const auto& mess : received)
    {
        if(mess.address == model().address())
        {
            // Scale min, max, and the value
            auto val = mess.value.val.toDouble();
            if(val < model().min())
                model().setMin(val);
            if(val > model().max())
                model().setMax(val);

            val = (val - model().min()) / (model().max() - model().min());

            // TODO this can be refactored easily
            // by having a function that returns a function
            // according to 0 / 1
            if(m_point == 0)
            {
                // Find first segment
                auto seg_it = std::find_if(
                            segs.begin(),
                            segs.end(),
                            [] (CurveSegmentModel& segt) {
                        return segt.start().x() == 0;
                });
                if(seg_it != segs.end())
                {
                    seg_it->setStart({0, val});
                }
            }
            else if(m_point == 1)
            {
                // Find last segment
                auto seg_it = std::find_if(
                            segs.begin(),
                            segs.end(),
                            [] (CurveSegmentModel& segt) {
                        return segt.end().x() == 1;
                });
                if(seg_it != segs.end())
                {
                    seg_it->setEnd({1, val});
                }
            }
            return;
        }
    }
}
