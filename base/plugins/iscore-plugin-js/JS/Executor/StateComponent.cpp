#include "StateComponent.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <JS/StateProcess.hpp>
#include <JS/Executor/JSAPIWrapper.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <ossia/editor/state/message.hpp>

namespace JS
{
namespace Executor
{
//// State ////
State::State(
        const QString& script,
        const Explorer::DeviceDocumentPlugin& devices):
    m_devices{devices.list()}
{
    // TODO find how to make it copyable ?
    m_engine = std::make_shared<QJSEngine>();
    m_engine->globalObject()
            .setProperty(
                "iscore",
                m_engine->newQObject(
                    new JS::APIWrapper{*m_engine, devices}
                    )
                );

    m_fun = m_engine->evaluate(script);
}

void State::operator()()
{
    if(!m_fun.isCallable())
        return;

    // Get the value of the js fun
    auto messages = JS::convert::messages(m_fun.call());

    m_engine->collectGarbage();

    for(const auto& mess : messages)
    {
        qDebug() << mess.toString();
        auto ossia_mess = iscore::convert::message(mess, m_devices);
        if(ossia_mess)
            ossia_mess->launch(); // TODO try to make a "state" convertible to message ?
    }
}

//// Component ////
StateProcessComponent::StateProcessComponent(
        RecreateOnPlay::StateElement& parentConstraint,
        JS::StateProcess& element,
        const RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    RecreateOnPlay::StateProcessComponent_T<JS::StateProcess>{
        parentConstraint, element, ctx, id, "JSStateComponent", parent}
{
    m_ossia_state = OSSIA::CustomState{State{element.script(), ctx.devices}};
}

OSSIA::StateElement StateProcessComponent::make(
        Process::StateProcess& proc,
        const RecreateOnPlay::Context& ctx)
{
    return OSSIA::CustomState{State{static_cast<const JS::StateProcess&>(proc).script(), ctx.devices}};
}

}
}
