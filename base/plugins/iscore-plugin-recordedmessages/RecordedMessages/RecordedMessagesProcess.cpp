#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <vector>

#include "Editor/Message.h"
#include "Editor/State.h"

#include "RecordedMessagesProcess.hpp"
#include <Editor/Clock.h>
#include <Editor/TimeConstraint.h>
#include <RecordedMessages/RecordedMessagesProcessModel.hpp>
namespace OSSIA {
class StateElement;
}  // namespace OSSIA


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

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state()
{
    return state(parent->getPosition());
}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state(double t)
{
    auto st = OSSIA::State::create();
    OSSIA::TimeConstraint& par_cst = *parent;

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
            qDebug() << mess.message.toString();
            auto ossia_mess = iscore::convert::message(mess.message, m_devices);
            if(ossia_mess)
                st->stateElements().push_back(ossia_mess);
        }
    }

    return st;
}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::offset(
        const OSSIA::TimeValue& off)
{
    return state(off / parent->getDurationNominal());
}

ProcessComponent::ProcessComponent(
        RecreateOnPlay::ConstraintElement& parentConstraint,
        RecordedMessages::ProcessModel& element,
        const RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    RecreateOnPlay::ProcessComponent{parentConstraint, element, id, "RecordedMessagesComponent", parent}
{
    auto proc = std::make_shared<ProcessExecutor>(ctx.devices, element.messages());
    m_ossia_process = proc;
}

const iscore::Component::Key& ProcessComponent::key() const
{
    static iscore::Component::Key k("RecordedMessagesComponent");
    return k;
}

ProcessComponentFactory::~ProcessComponentFactory()
{

}

RecreateOnPlay::ProcessComponent* ProcessComponentFactory::make(
        RecreateOnPlay::ConstraintElement& cst,
        Process::ProcessModel& proc,
        const RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent) const
{
    return new ProcessComponent{cst, static_cast<RecordedMessages::ProcessModel&>(proc), ctx, id, parent};
}

const ProcessComponentFactory::ConcreteFactoryKey&
ProcessComponentFactory::concreteFactoryKey() const
{
    static ConcreteFactoryKey k("e3b381f8-1cd3-4a85-bef7-27283447db50");
    return k;
}

bool ProcessComponentFactory::matches(
        Process::ProcessModel& proc,
        const RecreateOnPlay::DocumentPlugin&,
        const iscore::DocumentContext&) const
{
    return dynamic_cast<RecordedMessages::ProcessModel*>(&proc);
}

}
}
