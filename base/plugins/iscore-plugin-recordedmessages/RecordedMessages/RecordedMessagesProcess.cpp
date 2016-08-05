#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <vector>

#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>

#include "RecordedMessagesProcess.hpp"
#include <ossia/editor/scenario/clock.hpp>
#include <ossia/editor/scenario/time_constraint.hpp>
#include <RecordedMessages/RecordedMessagesProcessModel.hpp>


namespace RecordedMessages
{
namespace Executor
{
ProcessExecutor::ProcessExecutor(
        const Explorer::DeviceDocumentPlugin& devices,
        const RecordedMessagesList& lst):
    m_devices{devices.list()},
    m_list{lst}
{
    m_list.detach();
}

ossia::state_element ProcessExecutor::state()
{
    return state(parent->getPosition());
}

ossia::state_element ProcessExecutor::state(double t)
{
    ossia::state st;
    ossia::time_constraint& par_cst = *parent;

    auto cur_pos = t;
    auto span = par_cst.getGranularity();

    auto max_pos = cur_pos + span;

    // Look for all the messages

    int n = m_list.size();
    for(int i = 0; i < n; i++)
    {
        auto& mess = m_list[i];
        auto time = mess.percentage * par_cst.getDurationNominal();
        if(time >= cur_pos && time < max_pos)
        {
            st.add(iscore::convert::message(mess.message, m_devices));
        }
    }

    return st;
}

ossia::state_element ProcessExecutor::offset(
        ossia::time_value off)
{
    return state(off / parent->getDurationNominal());
}

Component::Component(
        RecreateOnPlay::ConstraintElement& parentConstraint,
        RecordedMessages::ProcessModel& element,
        const RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    ::RecreateOnPlay::ProcessComponent_T<RecordedMessages::ProcessModel, ProcessExecutor>{
          parentConstraint, element, ctx, id, "RecordedMessagesComponent", parent}
{
    m_ossia_process = new ProcessExecutor{ctx.devices, element.messages()};
}
}
}
