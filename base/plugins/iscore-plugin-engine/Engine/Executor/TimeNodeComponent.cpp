#include <ossia/editor/scenario/time_node.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <QDebug>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <exception>

#include "ConstraintComponent.hpp"
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
  connect(m_iscore_node.trigger(), &Scenario::TriggerModel::triggeredByGui,
          this, &TimeNodeComponent::on_GUITrigger);
}

void TimeNodeComponent::cleanup()
{
  m_ossia_node.reset();
}

ossia::expression_ptr TimeNodeComponent::makeTrigger() const
{
  auto& element = m_iscore_node;
  if (element.trigger() && element.trigger()->active())
  {
    try
    {
      return Engine::iscore_to_ossia::expression(
            element.trigger()->expression(), system().devices.list());
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
  m_ossia_node->setExpression(std::move(exp));
}

std::shared_ptr<ossia::time_node> TimeNodeComponent::OSSIATimeNode() const
{
  return m_ossia_node;
}

const Scenario::TimeNodeModel& TimeNodeComponent::iscoreTimeNode() const
{
  return m_iscore_node;
}

void TimeNodeComponent::on_GUITrigger()
{
  system().executionQueue.enqueue([node=m_ossia_node] {
    try
    {
      node->trigger();

      ossia::state accumulator;
      for (auto& event : node->timeEvents())
      {
        if (event->getStatus() == ossia::time_event::Status::HAPPENED)
          ossia::flatten_and_filter(accumulator, event->getState());
      }
      accumulator.launch();
    }
    catch (...)
    {
    }
  });
}
}
}
