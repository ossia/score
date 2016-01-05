#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <vector>

#include "Editor/Message.h"
#include "Editor/State.h"
#include "JSAPIWrapper.hpp"
#include "JSProcess.hpp"
#include <JS/JSProcessModel.hpp>
namespace OSSIA {
class StateElement;
}  // namespace OSSIA


namespace JS
{
namespace Executor
{
ProcessExecutor::ProcessExecutor(const DeviceDocumentPlugin& devices):
    m_devices{devices.list()},
    m_start{OSSIA::State::create()},
    m_end{OSSIA::State::create()}
{
    m_engine.globalObject().setProperty("iscore", m_engine.newQObject(new JS::APIWrapper{devices}));
}

void ProcessExecutor::setTickFun(const QString& val)
{
    m_tickFun = m_engine.evaluate(val);
}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state(
        const OSSIA::TimeValue& t,
        const OSSIA::TimeValue&)
{
    if(!m_tickFun.isCallable())
        return {};

    // 1. Convert the time in value.
    auto js_time = iscore::convert::JS::time(Ossia::convert::time(t));

    // 2. Get the value of the js fun
    auto messages = JS::convert::messages(m_tickFun.call({js_time}));

    m_engine.collectGarbage();

    auto st = OSSIA::State::create();
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

const ProcessComponentFactory::factory_key_type&
ProcessComponentFactory::key_impl() const
{
    static factory_key_type k("JSComponent");
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
