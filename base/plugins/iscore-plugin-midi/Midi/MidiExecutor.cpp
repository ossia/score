#include "MidiExecutor.hpp"
#include <Midi/MidiProcess.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/network/midi/detail/midi_impl.hpp>
#include <ossia/editor/scenario/time_constraint.hpp>

namespace Midi
{
namespace Executor
{
ProcessExecutor::ProcessExecutor(
        const Midi::ProcessModel& proc,
        const Device::DeviceList& devices):
    m_process{proc},
    m_devices{devices}
{
    // Load the address
    // Look for the real node in the device
    auto dev_p = devices.findDevice(proc.device());
    if(!dev_p)
        return;

    auto dev = dynamic_cast<Engine::Network::OSSIADevice*>(dev_p);
    if(!dev)
        return;

    // We get the node corresponding to the channel

    m_channelNode = dynamic_cast<ossia::net::midi::channel_node*>(
                Engine::iscore_to_ossia::findNodeFromPath(
    {QString::number(proc.channel())},
                    *dev->getDevice()));
}

ProcessExecutor::~ProcessExecutor()
{
}

ossia::state_element ProcessExecutor::state()
{
    return state(parent->getPosition());
}

ossia::state_element ProcessExecutor::state(double t)
{
    ossia::time_constraint& par_cst = *parent;

    ossia::time_value date = par_cst.getDate();
    if (date != mLastDate)
    {
        ossia::state st;
        if(mLastDate > date )
            mLastDate = 0;
        auto diff = (date - mLastDate) / par_cst.getDurationNominal();
        mLastDate = date;
        auto cur_pos = t;
        auto max_pos = cur_pos + diff;

        // Look for all the messages
        for(Note& note : m_process.notes)
        {
            auto start_time = note.start();
            auto end_time = note.end();

            //qDebug() << cur_pos << start_time << end_time << max_pos;
            if(start_time >= cur_pos && start_time < max_pos)
            {
                // Send note_on
                auto p = m_channelNode->note_on(note.pitch(), note.velocity());
                st.add(std::move(p[0]));
                st.add(std::move(p[1]));
            }
            if(end_time >= cur_pos && end_time < max_pos)
            {
                // Send note_on
                auto p = m_channelNode->note_off(note.pitch(), note.velocity());
                st.add(std::move(p[0]));
                st.add(std::move(p[1]));
            }
        }

        m_lastState = std::move(st);

    }
    //qDebug() << (double)par_cst.getGranularity() << (double)par_cst.getDurationNominal() << cur_pos << max_pos;

    return m_lastState;
}

ossia::state_element ProcessExecutor::offset(ossia::time_value off)
{
    return state(off / parent->getDurationNominal());
}


Component::Component(
        Engine::Execution::ConstraintElement& parentConstraint,
        Midi::ProcessModel& element,
        const Engine::Execution::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    ::Engine::Execution::ProcessComponent_T<Midi::ProcessModel, ProcessExecutor>{
          parentConstraint, element, ctx, id, "MidiComponent", parent}
{
    m_ossia_process = new ProcessExecutor{element, ctx.devices.list()};
}

}
}
