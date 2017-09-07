// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateComponent.hpp"
#include <ossia/editor/state/message.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <JS/Executor/JSAPIWrapper.hpp>
#include <JS/JSStateProcess.hpp>

namespace JS
{
namespace Executor
{
//// State ////
State::State(
    const QString& script, const Explorer::DeviceDocumentPlugin& devices)
    : m_devices{devices.list()}
{
  // TODO find how to make it copyable ?
  m_engine = std::make_shared<QJSEngine>();
  m_engine->globalObject().setProperty(
      "iscore", m_engine->newQObject(new JS::APIWrapper{*m_engine, devices}));

  m_fun = m_engine->evaluate(script);
}

void State::operator()()
{
  if (!m_fun.isCallable())
    return;

  // Get the value of the js fun
  auto messages = JS::convert::messages(m_fun.call());

  m_engine->collectGarbage();

  for (const auto& mess : messages)
  {
    qDebug() << mess.toString();
    auto ossia_mess = Engine::iscore_to_ossia::message(mess, m_devices);
    if (ossia_mess)
      ossia_mess
          ->launch(); // TODO try to make a "state" convertible to message ?
  }
}

//// Component ////
StateProcessComponent::StateProcessComponent(
    Engine::Execution::StateComponent& parentInterval,
    JS::StateProcess& element,
    const Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : Engine::Execution::StateProcessComponent_T<JS::StateProcess>{
          parentInterval, element, ctx, id, "JSStateComponent", parent}
{
  m_ossia_state = ossia::custom_state{State{element.script(), ctx.devices}};
}

ossia::state_element StateProcessComponent::make(
    Process::StateProcess& proc, const Engine::Execution::Context& ctx)
{
  return ossia::custom_state{
      State{static_cast<const JS::StateProcess&>(proc).script(), ctx.devices}};
}
}
}
