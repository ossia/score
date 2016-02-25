#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <vector>

#include "Editor/Message.h"
#include "Editor/State.h"
#include "JSAPIWrapper.hpp"
#include "Component.hpp"
#include <Editor/TimeConstraint.h>
#include <JS/JSProcessModel.hpp>
namespace OSSIA {
class StateElement;
}  // namespace OSSIA


namespace JS
{
namespace Executor
{
ProcessExecutor::ProcessExecutor(
        const Explorer::DeviceDocumentPlugin& devices):
    m_devices{devices.list()}
{
    m_engine.globalObject().setProperty("iscore", m_engine.newQObject(new JS::APIWrapper{m_engine, devices}));
}

void ProcessExecutor::setTickFun(const QString& val)
{
    m_tickFun = m_engine.evaluate(val);
}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state()
{
    return state(parent->getPosition());
}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state(double t)
{
    auto st = OSSIA::State::create();
    if(!m_tickFun.isCallable())
        return st;

    // 2. Get the value of the js fun
    auto messages = JS::convert::messages(m_tickFun.call({QJSValue{t}}));

    m_engine.collectGarbage();

    for(const auto& mess : messages)
    {
        //qDebug() << mess.toString();
        auto ossia_mess = iscore::convert::message(mess, m_devices);
        if(ossia_mess)
            st->stateElements().push_back(ossia_mess);
    }

    // 3. Convert our value back
    return st;
}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::offset(const OSSIA::TimeValue & off)
{
    return state(off / parent->getDurationNominal());
}

ProcessComponent::ProcessComponent(
        RecreateOnPlay::ConstraintElement& parentConstraint,
        JS::ProcessModel& element,
        const RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    RecreateOnPlay::ProcessComponent{parentConstraint, element, id, "JSComponent", parent}
{
    auto proc = std::make_shared<ProcessExecutor>(ctx.devices);
    proc->setTickFun(element.script());
    m_ossia_process = proc;
}

const iscore::Component::Key& ProcessComponent::key() const
{
    static iscore::Component::Key k("JSComponent");
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
    return new ProcessComponent{cst, static_cast<JS::ProcessModel&>(proc), ctx, id, parent};
}

const ProcessComponentFactory::ConcreteFactoryKey&
ProcessComponentFactory::concreteFactoryKey() const
{
    static ConcreteFactoryKey k("058245dd-9e56-4ca9-820c-ce0983c0bc44");
    return k;
}

bool ProcessComponentFactory::matches(
        Process::ProcessModel& proc,
        const RecreateOnPlay::DocumentPlugin&,
        const iscore::DocumentContext&) const
{
    return dynamic_cast<JS::ProcessModel*>(&proc);
}

}
}
