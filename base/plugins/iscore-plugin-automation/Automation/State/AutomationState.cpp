#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <algorithm>

#include <Automation/AutomationModel.hpp>
#include "AutomationState.hpp"
#include <Curve/CurveModel.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/Todo.hpp>

class QObject;
namespace Automation
{
ProcessState::ProcessState(
        ProcessModel& model,
        double watchedPoint,
        QObject* parent):
    ProcessStateDataInterface{model, parent},
    m_point{watchedPoint}
{
    ISCORE_ASSERT(0 <= watchedPoint && watchedPoint <= 1);

    con(this->process(), &ProcessModel::curveChanged,
            this, &ProcessStateDataInterface::stateChanged);

    con(this->process(), &ProcessModel::addressChanged,
            this, &ProcessStateDataInterface::stateChanged);
}

QString ProcessState::stateName() const
{ return "AutomationState"; }

// TESTME
::State::Message ProcessState::message() const
{
    ::State::Message m;
    m.address = process().address();
    for(const auto& seg : process().curve().segments())
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

double ProcessState::point() const
{ return m_point; }

ProcessState *ProcessState::clone(QObject* parent) const
{
    return new ProcessState{process(), m_point, parent};
}

ProcessModel& ProcessState::process() const
{
    return static_cast<ProcessModel&>(ProcessStateDataInterface::process());
}

std::vector<State::Address> ProcessState::matchingAddresses()
{
    // TODO have a better check of "address validity"
    if(!process().address().device.isEmpty())
        return {process().address()};
    return {};
}

::State::MessageList ProcessState::messages() const
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
::State::MessageList ProcessState::setMessages(
        const ::State::MessageList& received,
        const Process::MessageNode&)
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
            auto val = State::convert::value<float>(mess.value);
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
                            [] (Curve::SegmentModel& segt) {
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
                            [] (Curve::SegmentModel& segt) {
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
}
