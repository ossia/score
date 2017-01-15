#include "StateComponent.hpp"
#include <Engine/iscore2OSSIA.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <ossia/editor/scenario/time_event.hpp>

namespace Engine
{
namespace Execution
{ 
ISCORE_PLUGIN_ENGINE_EXPORT StateComponent::StateComponent(
    const Scenario::StateModel& element,
    const Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : Execution::Component{ctx, id, "Executor::State", nullptr}
    , m_state{Engine::iscore_to_ossia::state(element, ctx)}
{
}

ISCORE_PLUGIN_ENGINE_EXPORT void StateComponent::cleanup()
{
  m_ev.reset();
}

ISCORE_PLUGIN_ENGINE_EXPORT void StateComponent::onSetup(
    const std::shared_ptr<ossia::time_event>& root)
{
  m_ev = root;
  m_ev->addState(m_state);
}

ISCORE_PLUGIN_ENGINE_EXPORT void StateComponent::onDelete() const
{
  system().executionQueue.enqueue([ev=m_ev,st=m_state] {
    if(ev)
      ev->removeState(st);
  });
}

}
}
