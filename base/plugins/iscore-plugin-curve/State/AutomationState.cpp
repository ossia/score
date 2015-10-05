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

    con(this->process(), &AutomationModel::curveChanged,
            this, &ProcessStateDataInterface::stateChanged);

    con(this->process(), &AutomationModel::addressChanged,
            this, &ProcessStateDataInterface::stateChanged);
}

QString AutomationState::stateName() const
{ return "AutomationState"; }

// TESTME
iscore::Message AutomationState::message() const
{
    iscore::Message m;
    m.address = process().address();
    for(const CurveSegmentModel& seg : process().curve().segments())
    {
        // OPTIMIZEME introduce another index on that has an ordering on curve segments
        // to make this fast (just checking for the first and the last).
        if(seg.start().x() <= m_point && seg.end().x() >= m_point)
        {
            m.value.val = float(seg.valueAt(m_point) * (process().max() - process().min()) + process().min());
            return m;
        }
    }

    return {};
}

double AutomationState::point() const
{ return m_point; }

AutomationState *AutomationState::clone(QObject* parent) const
{
    return new AutomationState{process(), m_point, parent};
}

AutomationModel& AutomationState::process() const
{
    return static_cast<AutomationModel&>(ProcessStateDataInterface::process());
}

std::vector<iscore::Address> AutomationState::matchingAddresses()
{
    // TODO have a better check of "address validity"
    if(!process().address().device.isEmpty())
        return {process().address()};
    return {};
}

iscore::MessageList AutomationState::messages() const
{
    if(!process().address().device.isEmpty())
    {
        auto mess = message();
        if(!mess.address.device.isEmpty())
            return {mess};
    }

    return {};
}

// TESTME
iscore::MessageList AutomationState::setMessages(const iscore::MessageList& received, const MessageNode&)
{
    if(m_point != 0 && m_point != 1)
        return messages();

    const auto& segs = process().curve().segments();
    for(const auto& mess : received)
    {
        if(mess.address == process().address())
        {
            // Scale min, max, and the value
            // TODO convert to the real type of the curve.
            auto val = mess.value.val.toFloat();
            if(val < process().min())
                process().setMin(val);
            if(val > process().max())
                process().setMax(val);

            val = (val - process().min()) / (process().max() - process().min());

            if(m_point == 0)
            {
                // Find first segment
                // TODO ordering would help, here.
                auto seg_it = std::find_if(
                            segs.begin(),
                            segs.end(),
                            [] (CurveSegmentModel& segt) {
                        return segt.start().x() == 0;
                });
                if(seg_it != segs.end())
                {
                    if(val != seg_it->start().y())
                    {
                        seg_it->setStart({0, val});
                    }
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
                    if(val != seg_it->end().y())
                        seg_it->setEnd({1, val});
                }
            }
            return messages();
        }
    }
    return messages();
}
