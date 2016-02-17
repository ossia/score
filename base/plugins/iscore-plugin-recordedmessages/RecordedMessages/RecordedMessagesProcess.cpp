#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <vector>

#include "Editor/Message.h"
#include "Editor/State.h"

#include "RecordedMessagesProcess.hpp"
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
        const Explorer::DeviceDocumentPlugin& devices):
    m_devices{devices.list()}
{
}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state()
{
    auto st = OSSIA::State::create();
    return st;
}

ProcessComponent::ProcessComponent(
        RecreateOnPlay::ConstraintElement& parentConstraint,
        RecordedMessages::ProcessModel& element,
        const RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    RecreateOnPlay::ProcessComponent{parentConstraint, element, id, "RecordedMessagesComponent", parent}
{
    auto proc = std::make_shared<ProcessExecutor>(ctx.devices);
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
