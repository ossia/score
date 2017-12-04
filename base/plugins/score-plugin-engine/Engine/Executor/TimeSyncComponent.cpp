// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/scenario/time_sync.hpp>
#include <Engine/score2OSSIA.hpp>
#include <QDebug>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <exception>

#include "IntervalComponent.hpp"
#include "ScenarioComponent.hpp"
#include "TimeSyncComponent.hpp"
#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/detail/logger.hpp>
namespace Engine
{
namespace Execution
{
TimeSyncComponent::TimeSyncComponent(
    const Scenario::TimeSyncModel& element,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  :  Execution::Component{ctx, id, "Executor::Event", nullptr}
  , m_score_node{element}
{
  con(m_score_node, &Scenario::TimeSyncModel::triggeredByGui,
      this, &TimeSyncComponent::on_GUITrigger);

  con(m_score_node, &Scenario::TimeSyncModel::activeChanged,
      this, &TimeSyncComponent::updateTrigger);
  con(m_score_node, &Scenario::TimeSyncModel::triggerChanged,
      this, [this] (const State::Expression& expr) { this->updateTrigger(); });
}

void TimeSyncComponent::cleanup()
{
  m_ossia_node.reset();
}

ossia::expression_ptr TimeSyncComponent::makeTrigger() const
{
  auto& element = m_score_node;
  if (element.active())
  {
    try
    {
      return Engine::score_to_ossia::trigger_expression(
            element.expression(), system().devices.list());
    }
    catch (std::exception& e)
    {
      ossia::logger().error(e.what());
    }
  }

  return ossia::expressions::make_expression_true();
}

void TimeSyncComponent::onSetup(
    std::shared_ptr<ossia::time_sync> ptr,
    ossia::expression_ptr exp)
{
  m_ossia_node = ptr;
  m_ossia_node->set_expression(std::move(exp));
}

std::shared_ptr<ossia::time_sync> TimeSyncComponent::OSSIATimeSync() const
{
  return m_ossia_node;
}

const Scenario::TimeSyncModel& TimeSyncComponent::scoreTimeSync() const
{
  return m_score_node;
}

void TimeSyncComponent::updateTrigger()
{
  auto exp_ptr = std::make_shared<ossia::expression_ptr>( this->makeTrigger() );
  this->system().executionQueue.enqueue(
        [e = m_ossia_node, exp_ptr]
  {
    bool old = e->is_observing_expression();
    if(old)
      e->observe_expression(false);

    e->set_expression(std::move(*exp_ptr));

    if(old)
      e->observe_expression(true);

  });

}

void TimeSyncComponent::on_GUITrigger()
{
  this->system().executionQueue.enqueue(
        [e = m_ossia_node]
  {
    e->trigger_request = true;
  });
}


}
}
