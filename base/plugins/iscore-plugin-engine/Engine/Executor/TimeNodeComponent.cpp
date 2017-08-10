// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/scenario/time_node.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <QDebug>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <exception>

#include "ConstraintComponent.hpp"
#include "ScenarioComponent.hpp"
#include "TimeNodeComponent.hpp"
#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/detail/logger.hpp>

namespace Engine
{
namespace Execution
{
TimeNodeComponent::TimeNodeComponent(
    const Scenario::TimeNodeModel& element,
    const Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
  :  Execution::Component{ctx, id, "Executor::Event", nullptr}
  , m_iscore_node{element}
{
  con(m_iscore_node, &Scenario::TimeNodeModel::triggeredByGui,
      this, &TimeNodeComponent::on_GUITrigger);

  con(m_iscore_node, &Scenario::TimeNodeModel::activeChanged,
      this, &TimeNodeComponent::updateTrigger);
  con(m_iscore_node, &Scenario::TimeNodeModel::triggerChanged,
      this, [this] (const State::Expression& expr) { this->updateTrigger(); });
}

void TimeNodeComponent::cleanup()
{
  m_ossia_node.reset();
}

ossia::expression_ptr TimeNodeComponent::makeTrigger() const
{
  auto& element = m_iscore_node;
  if (element.active())
  {
    try
    {
      return Engine::iscore_to_ossia::trigger_expression(
            element.expression(), system().devices.list());
    }
    catch (std::exception& e)
    {
      ossia::logger().error(e.what());
    }
  }

  return ossia::expressions::make_expression_true();
}

void TimeNodeComponent::onSetup(
    std::shared_ptr<ossia::time_node> ptr,
    ossia::expression_ptr exp)
{
  m_ossia_node = ptr;
  m_ossia_node->set_expression(std::move(exp));
}

std::shared_ptr<ossia::time_node> TimeNodeComponent::OSSIATimeNode() const
{
  return m_ossia_node;
}

const Scenario::TimeNodeModel& TimeNodeComponent::iscoreTimeNode() const
{
  return m_iscore_node;
}

void TimeNodeComponent::updateTrigger()
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

void TimeNodeComponent::on_GUITrigger()
{
  if(dynamic_cast<Scenario::ProcessModel*>(this->iscoreTimeNode().parent()))
  {
    this->system().executionQueue.enqueue(
          [this,e = m_ossia_node]
    {
        e->trigger_request = true;
    });
  }
  else
  {
    this->system().executionQueue.enqueue(
          [this,e = m_ossia_node]
    {
        ossia::state st;
        e->trigger(st); //TODO refactor w/ loop
    });
  }

}
}
}
