#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <vector>

#include "Component.hpp"
#include "JSAPIWrapper.hpp"
#include <ossia/editor/scenario/time_constraint.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <JS/JSProcessModel.hpp>

namespace JS
{
namespace Executor
{
ProcessExecutor::ProcessExecutor(const Explorer::DeviceDocumentPlugin& devices)
    : m_devices{devices.list()}
{
  auto obj = m_engine.newQObject(new JS::APIWrapper{m_engine, devices});
  m_engine.globalObject().setProperty("iscore", obj);
}

void ProcessExecutor::setTickFun(const QString& val)
{
  m_tickFun = m_engine.evaluate(val);
  if (m_tickFun.isError())
    qDebug() << "Uncaught exception at line"
             << m_tickFun.property("lineNumber").toInt() << ":"
             << m_tickFun.toString();
}

ossia::state_element ProcessExecutor::state()
{
  return state(parent()->getDate() / parent()->getDurationNominal());
}

ossia::state_element ProcessExecutor::state(double t)
{
  ossia::state st;
  if (!m_tickFun.isCallable())
    return st;

  // 2. Get the value of the js fun
  auto messages = JS::convert::messages(m_tickFun.call({QJSValue{t}}));

  m_engine.collectGarbage();

  for (const auto& mess : messages)
  {
    st.add(Engine::iscore_to_ossia::message(mess, m_devices));
  }

  // 3. Convert our value back
  if(unmuted())
    return st;
  return {};
}

ossia::state_element ProcessExecutor::offset(ossia::time_value off)
{
  return state(off / parent()->getDurationNominal());
}

Component::Component(
    ::Engine::Execution::ConstraintComponent& parentConstraint,
    JS::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : ::Engine::Execution::
          ProcessComponent_T<JS::ProcessModel, ProcessExecutor>{
              parentConstraint, element, ctx, id, "JSComponent", parent}
{
  m_ossia_process = new ProcessExecutor{ctx.devices};
  OSSIAProcess().setTickFun(element.script());
}
}
}
