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
        //qDebug() << mess.toString();
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
    RecreateOnPlay::StateProcessComponent{parentConstraint, element, id, "JSStateComponent", parent}
{
    auto proc = std::make_shared<State>(element.script(), ctx.devices);
    m_ossia_process = proc;
}

const iscore::Component::Key& StateProcessComponent::key() const
{
    // TODO these should be uuids !!!!!
    static iscore::Component::Key k("3e46d422-6b69-4142-8500-a806b8a94284");
    return k;
}



//// Factory ////
StateProcessComponentFactory::~StateProcessComponentFactory()
{

}

RecreateOnPlay::StateProcessComponent* StateProcessComponentFactory::make(
        RecreateOnPlay::StateElement& cst,
        Process::StateProcess& proc,
        const RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent) const
{
    return new StateProcessComponent{cst, static_cast<JS::StateProcess&>(proc), ctx, id, parent};
}

const StateProcessComponentFactory::ConcreteFactoryKey&
StateProcessComponentFactory::concreteFactoryKey() const
{
    static ConcreteFactoryKey k("ff5a59c5-a710-46b5-a9c8-74d72a67a5d9");
    return k;
}

bool StateProcessComponentFactory::matches(
        Process::StateProcess& proc,
        const RecreateOnPlay::DocumentPlugin&,
        const iscore::DocumentContext&) const
{
    return dynamic_cast<JS::StateProcess*>(&proc);
}

std::shared_ptr<OSSIA::StateElement> StateProcessComponentFactory::make(
        Process::StateProcess &proc,
        const RecreateOnPlay::Context &ctx) const
{
    return  std::make_shared<State>(static_cast<const JS::StateProcess&>(proc).script(), ctx.devices);
}

}
}
