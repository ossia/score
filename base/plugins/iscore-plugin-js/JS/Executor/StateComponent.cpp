#include "StateComponent.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <JS/StateProcess.hpp>
#include <JS/Executor/JSAPIWrapper.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <Editor/Message.h>

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
    m_engine.globalObject()
            .setProperty(
                "iscore",
                m_engine.newQObject(
                    new JS::APIWrapper{m_engine, devices}
                    )
                );

    m_fun = m_engine.evaluate(script);
}

void State::launch() const
{
    if(!m_fun.isCallable())
        return;

    // Get the value of the js fun
    auto messages = JS::convert::messages(m_fun.call());

    m_engine.collectGarbage();

    for(const auto& mess : messages)
    {
        qDebug() << mess.toString();
        auto ossia_mess = iscore::convert::message(mess, m_devices);
        if(ossia_mess)
            ossia_mess->launch();
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
    auto proc = std::make_shared<State>(element.script(), ctx.devices);
    m_ossia_process = proc;
}

std::shared_ptr<OSSIA::StateElement> StateProcessComponent::make(
        Process::StateProcess& proc,
        const RecreateOnPlay::Context& ctx)
{
    return std::make_shared<State>(static_cast<const JS::StateProcess&>(proc).script(), ctx.devices);
}

}
}
